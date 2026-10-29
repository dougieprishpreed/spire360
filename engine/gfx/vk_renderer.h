/* Vulkan renderer: vulkan4xenon on the xSDK console build, any Vulkan 1.0
 * driver on a Linux host (offscreen, to check it against the software
 * renderer). The D3D9 renderer's design in Vulkan's terms:
 *
 *  - Each frame records one command buffer (two frames in flight, each
 *    with its region of a vertex ring, a uniform ring and a staging ring);
 *    vulkan4xenon replays it into the GPU's ring at vkQueueSubmit.
 *  - The canvas draws into the scene, an offscreen 1280x720 target; present()
 *    draws it to the display through the display ramp (set_gamma), which
 *    D3D9's gamma ramp did on the XDK build. Screen-reading shaders copy the
 *    scene, screenshots read it back.
 *  - Batches as D3D9 does: triangles collect while the state (texture,
 *    blend, flags, parameters, clip) is unchanged, then one vkCmdDraw.
 *  - The pixel shader is a variant per mode (shaders/vk: canvas.hlsl's
 *    ps_main split by its branches, and each of effects.hlsl's ps_e<id>,
 *    tools/xsdk/vk_shaders.py), compiled with the build: xost lowers loops
 *    and branches inside a shader badly (xsdk's docs/PITFALLS.md). Their
 *    parameters are the D3D9 renderer's pixel constants, one std140 block.
 *  - Canvas groups and captures (SubViewports) draw into targets of their
 *    own in render passes of their own; vulkan4xenon keeps the scene in
 *    eDRAM between them where both fit. */
#ifndef SPIRE_VK_RENDERER_H
#define SPIRE_VK_RENDERER_H

#include "effects.h"
#include "renderer.h"

namespace spire {

class VkRenderer : public Renderer {
public:
    static VkRenderer* create();
    virtual ~VkRenderer() {}
    /* width x height: the frame. display: show frames on the console's
     * display (present()); else the frames stay offscreen (host checks). */
    virtual bool init(int width, int height, bool display, std::string* error) = 0;
    /* After end(): the frame on the display, through the ramp. Offscreen,
     * the frame is submitted (read_frame reads it). */
    virtual void present() = 0;
    /* Each frame shown for this many vertical blanks (1 or 2). */
    virtual void set_present_interval(int vblanks) = 0;
    /* The display ramp: out = in^exponent, squeezed into 16-235 if limited
     * (the XDK build's D3D gamma ramp presets). */
    virtual void set_gamma(double exponent, bool limited) = 0;
    /* After end(): the frame as drawn (before the ramp), straight RGBA rows
     * top down; waits for the GPU. */
    virtual bool read_frame(std::vector<u8>& rgba) = 0;
    /* The frame as drawn to a 24-bit BMP (Back + RB). */
    virtual bool save_screenshot(const char* path) = 0;
    /* After end(): submits the frame without showing it and waits for the
     * GPU (profile=1's GPU probe), as wait_idle() waits for what came before. */
    virtual void finish() = 0;
    virtual void wait_idle() = 0;
    virtual long draw_calls() const = 0;
    /* Time in the draw calls since the last take (ms): setting their state,
     * and recording the draw (copying its vertices). */
    virtual void take_draw_times(double* state_ms, double* submit_ms) = 0;
    /* 1: canvas groups off (their children drawn unmasked), 0 normal. */
    enum { GROUP_DEBUG_MODES = 2 };
    virtual void set_group_debug(int mode) = 0;
    virtual int group_debug() const = 0;
    /* Shaders whose module or pipeline failed (each logged once). */
    virtual int failed_shaders() const = 0;
};

} /* namespace spire */

#endif
