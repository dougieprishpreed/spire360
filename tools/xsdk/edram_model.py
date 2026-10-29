#!/usr/bin/env python3
"""A model of vulkan4xenon's eDRAM placement (its src/queue.c: begin_render_pass, end_render_pass, color_tile,
forget_colors, keep_colors, end_color_clear) for one colour target and no depth, as the Vulkan renderer's passes are:
what a frame's passes cost in draw-backs, clear resolves, resolves and resolves put off, which the console's
profile=1 log counts ("vulkan4xenon: ... images drawn back into eDRAM").

    tools/xsdk/edram_model.py

runs an Underdocks fight's frame (the scene, 11 canvas groups, the display) with its group passes begun with a clear
(the first xSDK builds: each group's begin had the scene resolved with that clear and drawn back after, as the
console counted, 11 a frame) and begun fresh with the area cleared in the pass (vk_renderer.cpp's ensure_pass now: the
scene's resolve put off, nothing drawn back). A change to vulkan4xenon's placement wants the model changed too."""
import math

TILES = 2048
COLORS = 8


def tiles(w, h):
    return math.ceil(w / 80) * math.ceil(h / 16)


def contains(outer, inner):
    return (inner[0] >= outer[0] and inner[1] >= outer[1] and inner[0] + inner[2] <= outer[0] + outer[2]
            and inner[1] + inner[3] <= outer[1] + outer[3])


class Image:
    def __init__(self, name, w, h):
        self.name, self.w, self.h = name, w, h


def unknown():
    return {'contents': None}


class Queue:
    def __init__(self):
        self.colors = [unknown() for _ in range(COLORS)]
        self.counts = dict(drawn_back=0, drawn_back_px=0, clear_resolves=0, resolves=0, resolve_px=0, put_off=0,
                           late=0, late_px=0)

    def part_tiles(self, p):
        return tiles(p['w'], p['h'])

    def overlaps(self, p, first, n):
        return p['contents'] is not None and p['tile'] < first + n and first < p['tile'] + self.part_tiles(p)

    def tiles_unkept(self, first, n):
        return all(not self.overlaps(p, first, n) for p in self.colors)

    def color_tile(self, ps):
        img, n = ps['image'], tiles(ps['image'].w, ps['image'].h)
        for p in self.colors:
            if p['contents'] is None or p['w'] != img.w or p['h'] != img.h:
                continue
            if ps['load'] == 'clear':
                serves = p['contents'] == 'cleared' and p['rgba'] == ps['rgba']
            else:
                serves = p['contents'] == 'image' and p['image'] is img
            if serves:
                return p['tile']
        best = None
        cands = [0] + [p['tile'] + self.part_tiles(p) for p in self.colors if p['contents'] is not None]
        for f in cands:
            if (best is None or f < best) and f + n <= TILES and self.tiles_unkept(f, n):
                best = f
        return 0 if best is None else best

    def lose(self, p):
        if p.get('unresolved'):
            self.counts['late'] += 1
            self.counts['late_px'] += p['w'] * p['h']
        p.clear()
        p['contents'] = None

    def begin(self, ps):
        img = ps['image']
        n = tiles(img.w, img.h)
        tile = self.color_tile(ps)
        ps['tile'] = tile
        held = next((dict(p) for p in self.colors if p['contents'] is not None and p['tile'] == tile), unknown())
        if held['contents'] == 'image' and held['image'] is not img:
            held = unknown()
        # forget_colors
        for p in self.colors:
            if not (self.overlaps(p, tile, n) or (p['contents'] == 'image' and p['image'] is img)):
                continue
            own = p['contents'] == 'image' and p['image'] is img and p['tile'] == tile
            if own:
                p.clear(); p['contents'] = None
            else:
                self.lose(p)
        area = ps['area']
        if ps['load'] == 'load':
            ready = held['contents'] == 'image' and held['image'] is img and contains(held['area'], area)
            if not ready:
                self.counts['drawn_back'] += 1
                self.counts['drawn_back_px'] += area[2] * area[3]
                held = dict(contents='image', image=img, w=img.w, h=img.h, tile=tile, area=area)
        elif ps['load'] == 'clear':
            ready = (held['contents'] == 'cleared' and held['w'] == img.w and held['h'] == img.h
                     and held['rgba'] == ps['rgba'] and contains(held['area'], area))
            if not ready:
                self.counts['clear_resolves'] += 1
                held = dict(contents='cleared', w=img.w, h=img.h, tile=tile, rgba=ps['rgba'], area=area)
        else:  # don't care
            if held['contents'] is None:
                held = dict(contents='image', image=img, w=img.w, h=img.h, tile=tile, area=area)
        # keep_colors: the pass's part first, then the others, newest first
        rest = [p for p in self.colors if p['contents'] is not None]
        kept = [held] + rest
        for p in kept[COLORS:]:
            self.lose(p)
        self.colors = (kept + [unknown() for _ in range(COLORS)])[:COLORS]

    def end(self, ps, nxt, reads_before_next):
        img = ps['image']
        whole = (0, 0, img.w, img.h)
        area = ps['area']
        part = self.colors[0]
        whole_put_off = part.get('unresolved') and not contains(area, whole)
        if whole_put_off:
            self.counts['late'] += 1
            self.counts['late_px'] += img.w * img.h
            part['unresolved'] = False
        # end_color_clear
        clear_color = False
        rgba = None
        if nxt is not None:
            ni = nxt['image']
            if ni.w == img.w and ni.h == img.h and contains(area, nxt['area']):
                clear_color = nxt['load'] == 'clear'
                rgba = nxt.get('rgba')
        stored = part['area'] if (part['contents'] == 'image' and part.get('image') is img
                                  and contains(part['area'], area)) else area
        waits = not clear_color and contains(area, whole) and not reads_before_next
        if not waits:
            self.counts['resolves'] += 1
            self.counts['resolve_px'] += area[2] * area[3]
        else:
            self.counts['put_off'] += 1
        if clear_color:
            self.colors[0] = dict(contents='cleared', w=img.w, h=img.h, tile=ps['tile'], rgba=rgba, area=area)
        else:
            self.colors[0] = dict(contents='image', image=img, w=img.w, h=img.h, tile=ps['tile'], area=stored,
                                  unresolved=waits)

    def end_buffer(self):
        for p in self.colors:
            if p.get('unresolved'):
                self.counts['late'] += 1
                self.counts['late_px'] += p['w'] * p['h']
                p['unresolved'] = False


def run(frames, frame_passes):
    q = Queue()
    per = []
    for f in range(frames):
        passes = frame_passes(f)
        before = dict(q.counts)
        for i, ps in enumerate(passes):
            q.begin(ps)
            nxt = passes[i + 1] if i + 1 < len(passes) else None
            q.end(ps, nxt, ps.get('read_before_next', False))
        q.end_buffer()
        per.append({k: q.counts[k] - before[k] for k in q.counts})
    return per


W, H = 1280, 720
SCENE = Image('scene', W, H)
DISPLAY = [Image('display%d' % i, W, H) for i in range(2)]
AREAS = [(0, 256, 1280, 464), (0, 0, 1280, 720), (0, 96, 1280, 640), (0, 32, 1280, 704), (0, 0, 1280, 720),
         (0, 256, 1280, 608), (0, 0, 1280, 720), (0, 32, 1280, 800 - 96), (0, 0, 1280, 720), (0, 96, 1280, 624),
         (0, 0, 1280, 720)]


def underdocks(group_image, group_load, groups=11):
    def frame(f):
        ps = [dict(image=SCENE, load='clear', rgba=(0, 0, 0, 1), area=(0, 0, W, H))]
        for g in range(groups):
            a = AREAS[g % len(AREAS)]
            a = (a[0], a[1], a[2], min(a[3], H - a[1]))
            if group_image.w != W or group_image.h != H:
                a = (a[0], a[1], min(a[2], group_image.w - a[0]), min(a[3], group_image.h - a[1]))
            ps.append(dict(image=group_image, load=group_load, rgba=(0, 0, 0, 0), area=a,
                           read_before_next=True))
            ps.append(dict(image=SCENE, load='load', area=(0, 0, W, H)))
        ps[-1]['read_before_next'] = True  # the present samples the scene
        ps.append(dict(image=DISPLAY[f % 2], load='dontcare', area=(0, 0, W, H), read_before_next=True))
        return ps
    return frame


if __name__ == '__main__':
    for label, gimg, load in (('group passes begun with a clear', Image('group', W, H), 'clear'),
                              ('group passes begun with a load', Image('group', W, H), 'load'),
                              ('group passes begun fresh', Image('group', W, H), 'dontcare')):
        per = run(4, underdocks(gimg, load))
        print(label)
        for f, c in enumerate(per):
            print('  frame %d: drawn back %d (%.1f screens), clear resolves %d, resolves %d (%.1f screens), '
                  'put off %d, late %d (%.1f screens)' % (f, c['drawn_back'], c['drawn_back_px'] / (W * H),
                  c['clear_resolves'], c['resolves'], c['resolve_px'] / (W * H), c['put_off'], c['late'],
                  c['late_px'] / (W * H)))
