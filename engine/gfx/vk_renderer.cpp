#include "vk_renderer.h"

#include "../core/pack.h"

#include <vulkan/vulkan.h>
#if defined(__XSDK__)
#include <vulkan4xenon.h>
#include <xsdk/present.h>
#endif
#include "vk_shader_table.h" /* the build's SPIR-V (tools/xsdk/vk_shaders.py) */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <map>

/* In C++98 on 64-bit hosts VK_NULL_HANDLE is ((void*)0), which C++ won't
 * turn into a handle; 0 becomes any handle, pointer or 64-bit integer. */
#undef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0

namespace spire {

namespace {

enum { FRAMES = 2 };
/* Per frame, in GPU memory; a frame that fills one submits what it has and
 * waits for the GPU. The sandpit's sand draws ~130,000 vertices a frame. */
const VkDeviceSize VERTEX_RING = 6 * 1024 * 1024;
const VkDeviceSize UNIFORM_RING = 2 * 1024 * 1024;
/* texture uploads, in the CPU's cached memory where the device has it */
const VkDeviceSize STAGING_RING = 4 * 1024 * 1024;
const VkFormat TARGET_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;
/* the D3D9 renderer's pixel constants c0..c45, and the radial blur's count */
enum { PARAM_VECS = 46, PARAM_BYTES = PARAM_VECS * 16 + 16 };
/* the extra samplers (set 2): canvas.hlsl's s1..s12 but s3..s8 shared
 * between the fire and the effects */
enum { X_GROUP = 0, X_LUT = 1, X_TEX0 = 2, X_SCREEN_A = 8, X_SCREEN_B = 9, X_SCREEN_NOISE = 10, X_SCREEN_MASK = 11, X_COUNT = 12 };

/* GCC turns small copies into 8-byte stores, which fault in the GPU's
 * write-combined memory unless 8-byte aligned; xsdk's memcpy keeps to the
 * stores that memory takes (xsdk docs/PITFALLS.md), so call it, never
 * inline it. */
void* (*volatile g_memcpy)(void*, const void*, size_t) = memcpy;
inline void gpu_copy(void* dst, const void* src, size_t n) { g_memcpy(dst, src, n); }

VkDeviceSize align_up(VkDeviceSize v, VkDeviceSize a) { return (v + a - 1) & ~(a - 1); }

double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* Host checks: SPIRE_VK_TRACE=1 logs each pass and draw (what a frame
 * records). Never on the console. */
bool trace_on() {
#if defined(__XSDK__)
    return false;
#else
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("SPIRE_VK_TRACE");
        on = e && e[0] == '1' ? 1 : 0;
    }
    return on == 1;
#endif
}

bool vk_ok(VkResult r, const char* what) {
    if (r == VK_SUCCESS) return true;
    log_warn("vk: %s failed (%d)", what, (int)r);
    return false;
}

/* The pixel shader variants: canvas.hlsl's ps_main by its branches, the
 * level pass, the copy and present passes, main.frag's modes one at a time,
 * then effect id at V_EFFECT + id. xost runs every instruction of both sides
 * of a branch or select, so a mode in a shader of its own costs a draw only
 * its own instructions. */
enum Variant {
    V_PLAIN, V_TEXT, V_MAIN, V_FIRE, V_VFX, V_SCREEN_DARK, V_SCREEN_NOISE, V_SCREEN_LEVEL, V_SCREEN_RADIAL,
    V_SCREEN_GLASS, V_SCREEN_DOOM, V_SCREEN_DISTORT, V_SCREEN_CHROMA, V_SCREEN_AT_UV, V_COPY, V_PRESENT,
    V_DOOM_BAR, V_VFX_POLAR, V_GROUP, V_COLOR_MATRIX, V_GRADIENT_MAP, V_RIPPLE, V_EFFECT,
    V_COUNT = V_EFFECT + EFFECT_COUNT
};

const char* const VARIANT_FILES[V_EFFECT] = { "plain.frag",          "text.frag",          "main.frag",
                                              "fire.frag",           "vfx.frag",           "screen_dark.frag",
                                              "screen_noise.frag",   "screen_level.frag",  "screen_radial.frag",
                                              "screen_glass.frag",   "screen_doom.frag",   "screen_distort.frag",
                                              "screen_chroma.frag",  "screen_at_uv.frag",  "copy.frag",
                                              "present.frag",        "doom_bar.frag",      "vfx_polar.frag",
                                              "group.frag",          "color_matrix.frag",  "gradient_map.frag",
                                              "ripple.frag" };

/* Blend states: the D3D9 renderer's (one set of factors for colour and
 * alpha), and none (the passes that replace what they draw over). */
enum { BLEND_NONE = 5, BLEND_KINDS = 6 };
/* the extra samplers' sets a frame's pool holds */
enum { EXTRA_SETS = 1024 };
/* Time stamps: two a frame in flight (its first command and its last), then
 * a traced frame's, one after each draw, pass begin and pass end. */
enum { TRACE_BASE = 2 * FRAMES, TRACE_MAX = 4000 };
/* What a traced frame's time stamp follows */
enum { MARK_START, MARK_DRAW, MARK_QUAD, MARK_BEGIN, MARK_END, MARK_UPLOAD, MARK_RESUME };
/* the targets a pass draws into, for the trace and the counts */
enum { TARGET_SCENE, TARGET_GROUP, TARGET_CAPTURE, TARGET_SCREEN, TARGET_DISPLAY, TARGET_KINDS };
const char* const TARGET_NAMES[TARGET_KINDS] = { "scene", "group", "capture", "screen copy", "display" };
const char* const LOAD_NAMES[3] = { "clear", "load", "fresh" };

struct Buffer {
    VkBuffer buf;
    VkDeviceMemory mem;
    u8* map;
    VkDeviceSize size;
    Buffer() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE), map(NULL), size(0) {}
};

class VkTexture : public Texture {
public:
    VkTexture() : image(VK_NULL_HANDLE), mem(VK_NULL_HANDLE), view(VK_NULL_HANDLE), fb(VK_NULL_HANDLE),
                  layout(VK_IMAGE_LAYOUT_UNDEFINED), swapchain(false) {
        for (int i = 0; i < 4; ++i) sets[i] = VK_NULL_HANDLE;
    }
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFramebuffer fb;      /* targets */
    VkImageLayout layout;  /* after the commands recorded so far */
    bool swapchain;
    VkDescriptorSet sets[4]; /* by sampler: 1 repeat, 2 nearest */
};

struct Frame {
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore acquired, rendered;
    VkDescriptorPool pool; /* the extra samplers' sets, reset with the frame */
    std::vector<VkTexture*> dead;
    std::vector<Buffer> spare; /* big uploads' staging, freed with the frame */
    bool timed;                /* its two time stamps written */
    Frame() : cmd(VK_NULL_HANDLE), fence(VK_NULL_HANDLE), acquired(VK_NULL_HANDLE), rendered(VK_NULL_HANDLE),
              pool(VK_NULL_HANDLE), timed(false) {}
};

/* A traced frame's time stamp: what it follows */
struct Mark {
    int kind;    /* MARK_* */
    int variant; /* a draw's shader; a pass's target (TARGET_*) */
    int blend;   /* a draw's blend; a pass begin's load (LOAD_NAMES) */
    int label;   /* the item drawing (an index into the labels), -1 none */
    int vertices;
    int w, h;    /* the texture's size */
};

class Impl : public VkRenderer {
public:
    Impl();
    virtual ~Impl();
    virtual bool init(int width, int height, bool display, std::string* error);
    virtual Texture* create_texture(const TextureData& data);
    virtual void destroy_texture(Texture* t);
    virtual void begin(int width, int height, const Color& clear);
    virtual void set_clip(const Rect2* clip);
    virtual void draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags);
    virtual Vertex* reserve(Texture* tex, int count, BlendMode blend, int flags);
    virtual void commit() {}
    virtual void set_msdf(float px_range, float outline);
    virtual void set_color_matrix(const float* m);
    virtual void set_ripple(const float* p);
    virtual void set_gradient_map(Texture* lut, bool alpha_from_red);
    virtual void set_fire(const FireParams* p);
    virtual void set_vfx(const VfxParams* p);
    virtual bool set_screen(const ScreenParams* p);
    virtual bool set_effect(const EffectParams* e);
    virtual void screen_barrier() { screen_levels_ = 0; }
    virtual bool begin_group(const Rect2& bounds);
    virtual void end_group();
    virtual bool begin_capture(int w, int h);
    virtual Texture* end_capture(Texture* reuse);
    virtual void end();
    virtual void present();
    virtual void set_present_interval(int vblanks) { interval_ = vblanks >= 2 ? 2 : 1; }
    virtual void set_gamma(double exponent, bool limited);
    virtual bool read_frame(std::vector<u8>& rgba);
    virtual bool save_screenshot(const char* path);
    virtual void finish();
    virtual void wait_idle();
    virtual long draw_calls() const { return draws_; }
    virtual void take_draw_times(double* state_ms, double* submit_ms) {
        *state_ms = state_ms_;
        *submit_ms = submit_ms_;
        state_ms_ = submit_ms_ = 0;
    }
    virtual void set_group_debug(int mode) {
        group_debug_ = mode;
        log_info("vk: canvas group debug mode %d", mode);
    }
    virtual int group_debug() const { return group_debug_; }
    virtual int failed_shaders() const { return failed_; }
    virtual void set_timing(bool on) { timing_ = on && queries_; }
    virtual void take_frame_stats(FrameStats* s) {
        *s = stats_;
        memset(&stats_, 0, sizeof(stats_));
    }
    virtual double last_frame_gpu_ms() const { return last_gpu_ms_; }
    virtual void trace_next_frame(const char* why) {
        if (!queries_) return;
        trace_pending_ = true;
        trace_why_ = why;
    }
    virtual LayerCounts layer_counts() const;
    virtual bool traced() const { return tracing_; }
    virtual void trace_label(const char* label);

private:
    /* ---- set-up ---- */
    bool create_instance();
    bool pick_device();
    bool create_device(bool display);
    bool create_swapchain();
    bool create_common();
    u32 memory_type(u32 bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid);
    bool make_buffer(Buffer& b, VkDeviceSize size, VkBufferUsageFlags usage, bool cached);
    void destroy_buffer(Buffer& b);
    bool make_image(VkTexture* t, VkFormat format, VkImageUsageFlags usage);
    bool make_view(VkTexture* t, VkFormat format);
    bool make_framebuffer(VkTexture* t);
    VkTexture* make_target(int w, int h);
    void free_texture(VkTexture* t);
    VkRenderPass render_pass(VkAttachmentLoadOp load, VkImageLayout initial, VkImageLayout final);
    VkShaderModule module(const char* file);
    VkPipeline pipeline(int variant, int blend);
    /* ---- frames ---- */
    void begin_frame();
    void begin_commands();
    void release_garbage(Frame& f);
    void submit(bool wait);
    void flush_and_wait();
    /* ---- passes ---- */
    void open_pass(VkTexture* t, bool clear, const float* clear_color, const VkRect2D* area, bool fresh = false);
    void ensure_pass();
    void end_pass();
    void barrier(VkTexture* t, VkImageLayout to);
    void set_target_constants(int w, int h);
    void draw_quad(VkTexture* tex, int variant, int blend, int sampler, int w, int h);
    /* ---- textures ---- */
    bool upload(VkTexture* t, const u8* data, size_t bytes, int w, int h, VkFormat format);
    VkDescriptorSet texture_set(VkTexture* t, int sampler);
    VkDescriptorSet extra_set(VkTexture* const* tex, const int* samplers);
    /* ---- draws ---- */
    bool room(int vertices) const;
    void flush();
    void apply_state();
    int variant() const;
    bool copy_screen();
    bool screen_pass(int level);
    void level_size(int level, int* w, int* h) const;
    void release_screen();
    /* ---- timing ---- */
    int target_kind(const VkTexture* t) const;
    void mark(int kind, int variant, int blend, int vertices, const VkTexture* tex);
    void trace_report();

    /* Vulkan */
    VkInstance instance_;
    VkPhysicalDevice phys_;
    VkDevice dev_;
    VkQueue queue_;
    u32 queue_family_;
    VkPhysicalDeviceMemoryProperties mem_props_;
    VkDeviceSize uniform_align_;
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapchain_;
    std::vector<VkTexture*> swap_images_;
    u32 image_index_;
    bool display_;
    VkCommandPool cmd_pool_;
    VkDescriptorSetLayout layouts_[3];
    VkPipelineLayout pipe_layout_;
    VkDescriptorPool tex_pool_;
    VkDescriptorSet uniform_set_;
    VkSampler samplers_[4];
    std::map<u32, VkRenderPass> passes_;
    VkRenderPass compat_pass_;
    VkShaderModule vert_;
    VkShaderModule frag_[V_COUNT];
    bool frag_failed_[V_COUNT];
    VkPipeline pipes_[V_COUNT][BLEND_KINDS];
    bool pipe_failed_[V_COUNT][BLEND_KINDS];
    int failed_;
    Frame frames_[FRAMES];
    int frame_;
    bool frame_started_;
    Buffer vertex_ring_, uniform_ring_, staging_ring_, readback_;
    VkDeviceSize vertex_used_, uniform_used_, staging_used_;
    /* the scene, its copy's levels, the group layer, the display ramp */
    VkTexture* scene_;
    VkTexture* white_;
    VkTexture* ramp_;
    VkTexture* group_;
    enum { SCREEN_LEVELS = 6 };
    VkTexture* screen_lv_[SCREEN_LEVELS];
    std::map<u32, VkTexture*> captures_; /* by size: what a capture draws into */
    /* the open render pass */
    VkTexture* pass_tex_;
    bool pass_open_;
    VkTexture* target_;     /* where draws go: the scene, the group layer or a capture */
    bool target_clear_;     /* a group's or capture's target is cleared when its pass begins */
    bool scene_clear_;      /* the scene is cleared when its first pass begins */
    float clear_[4];        /* the scene's clear colour */
    VkRect2D target_area_;  /* the pass's render area (a group: its bounds) */
    int target_w_, target_h_;
    VkPipeline bound_pipe_;
    VkDescriptorSet bound_tex_set_, bound_extra_set_;
    int extra_sets_; /* allocated from the frame's pool */
    /* the uniform block the draws read: its offset, and what it holds */
    float params_[PARAM_VECS * 4 + 4];
    u32 params_offset_;
    bool params_valid_;
    VkDescriptorSet extra_set_;
    /* the batch, in the CPU's own memory until a draw copies it out */
    enum { BATCH_MAX = RESERVE_MAX };
    Vertex* batch_;
    int batch_count_;
    /* the pending state (what the batch was recorded with) */
    VkTexture* tex_;
    BlendMode blend_;
    int flags_;
    float msdf_[2];
    bool cm_on_;
    float cm_[12];
    bool ripple_on_;
    float ripple_[4];
    VkTexture* gm_;
    bool gm_red_;
    bool fire_on_;
    FireParams fire_;
    bool vfx_on_;
    VfxParams vfx_;
    bool clip_on_;
    int clip_[4]; /* left, top, right, bottom */
    bool dirty_;
    bool screen_on_;
    ScreenParams screen_;
    int screen_levels_;
    bool screen_failed_;
    bool effect_on_;
    EffectParams effect_;
    bool in_group_, group_failed_;
    int cap_w_, cap_h_;
    int group_debug_;
    int width_, height_;
    int interval_;
    u32 last_vblank_;
    long draws_;
    double state_ms_, submit_ms_;
    bool frame_ended_;
    /* GPU timing (set_timing) and the traced frame (trace_next_frame) */
    VkQueryPool queries_;
    double tick_ns_;
    bool timing_;
    FrameStats stats_;
    double last_gpu_ms_;
    bool trace_pending_, tracing_;
    std::string trace_why_;
    std::vector<Mark> marks_;
    std::vector<std::string> labels_;
    int label_;
    LayerCounts trace_counts_; /* vulkan4xenon's when the traced frame began */
};

} /* namespace */

VkRenderer* VkRenderer::create() { return new Impl(); }

namespace {

Impl::Impl()
    : instance_(VK_NULL_HANDLE), phys_(VK_NULL_HANDLE), dev_(VK_NULL_HANDLE), queue_(VK_NULL_HANDLE), queue_family_(0),
      uniform_align_(256), surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE), image_index_(0), display_(false),
      cmd_pool_(VK_NULL_HANDLE), pipe_layout_(VK_NULL_HANDLE), tex_pool_(VK_NULL_HANDLE), uniform_set_(VK_NULL_HANDLE),
      compat_pass_(VK_NULL_HANDLE), vert_(VK_NULL_HANDLE), failed_(0), frame_(0), frame_started_(false), vertex_used_(0),
      uniform_used_(0), staging_used_(0), scene_(NULL), white_(NULL), ramp_(NULL), group_(NULL), pass_tex_(NULL),
      pass_open_(false), target_(NULL), target_clear_(false), scene_clear_(false), target_w_(0), target_h_(0), bound_pipe_(VK_NULL_HANDLE),
      bound_tex_set_(VK_NULL_HANDLE), bound_extra_set_(VK_NULL_HANDLE), extra_sets_(0), params_offset_(0),
      params_valid_(false), extra_set_(VK_NULL_HANDLE), batch_(new Vertex[BATCH_MAX]), batch_count_(0), tex_(NULL),
      blend_(BLEND_MIX), flags_(0), cm_on_(false), ripple_on_(false), gm_(NULL), gm_red_(false), fire_on_(false),
      vfx_on_(false), clip_on_(false), dirty_(true), screen_on_(false), screen_levels_(0), screen_failed_(false),
      effect_on_(false), in_group_(false), group_failed_(false), cap_w_(0), cap_h_(0), group_debug_(0), width_(0),
      height_(0), interval_(1), last_vblank_(0), draws_(0), state_ms_(0), submit_ms_(0), frame_ended_(false),
      queries_(VK_NULL_HANDLE), tick_ns_(1), timing_(false), last_gpu_ms_(0), trace_pending_(false), tracing_(false),
      label_(-1) {
    memset(&stats_, 0, sizeof(stats_));
    memset(&trace_counts_, 0, sizeof(trace_counts_));
    for (int i = 0; i < 3; ++i) layouts_[i] = VK_NULL_HANDLE;
    for (int i = 0; i < 4; ++i) samplers_[i] = VK_NULL_HANDLE;
    for (int v = 0; v < V_COUNT; ++v) {
        frag_[v] = VK_NULL_HANDLE;
        frag_failed_[v] = false;
        for (int b = 0; b < BLEND_KINDS; ++b) {
            pipes_[v][b] = VK_NULL_HANDLE;
            pipe_failed_[v][b] = false;
        }
    }
    for (int i = 0; i < SCREEN_LEVELS; ++i) screen_lv_[i] = NULL;
    memset(params_, 0, sizeof(params_));
    memset(clear_, 0, sizeof(clear_));
    memset(&target_area_, 0, sizeof(target_area_));
    memset(&fire_, 0, sizeof(fire_));
    memset(&vfx_, 0, sizeof(vfx_));
    memset(&screen_, 0, sizeof(screen_));
    memset(&effect_, 0, sizeof(effect_));
    msdf_[0] = 16;
    msdf_[1] = 0;
    memset(cm_, 0, sizeof(cm_));
    memset(ripple_, 0, sizeof(ripple_));
    memset(clip_, 0, sizeof(clip_));
}

Impl::~Impl() {
    if (dev_) {
        vkDeviceWaitIdle(dev_);
        for (int f = 0; f < FRAMES; ++f) release_garbage(frames_[f]);
        for (std::map<u32, VkTexture*>::iterator it = captures_.begin(); it != captures_.end(); ++it)
            free_texture(it->second);
        release_screen();
        VkTexture* own[4] = { scene_, white_, ramp_, group_ };
        for (int i = 0; i < 4; ++i)
            if (own[i]) free_texture(own[i]);
        for (size_t i = 0; i < swap_images_.size(); ++i) free_texture(swap_images_[i]);
        for (int v = 0; v < V_COUNT; ++v) {
            for (int b = 0; b < BLEND_KINDS; ++b)
                if (pipes_[v][b]) vkDestroyPipeline(dev_, pipes_[v][b], NULL);
            if (frag_[v]) vkDestroyShaderModule(dev_, frag_[v], NULL);
        }
        if (vert_) vkDestroyShaderModule(dev_, vert_, NULL);
        for (std::map<u32, VkRenderPass>::iterator it = passes_.begin(); it != passes_.end(); ++it)
            vkDestroyRenderPass(dev_, it->second, NULL);
        for (int i = 0; i < 4; ++i)
            if (samplers_[i]) vkDestroySampler(dev_, samplers_[i], NULL);
        destroy_buffer(vertex_ring_);
        destroy_buffer(uniform_ring_);
        destroy_buffer(staging_ring_);
        destroy_buffer(readback_);
        for (int f = 0; f < FRAMES; ++f) {
            if (frames_[f].pool) vkDestroyDescriptorPool(dev_, frames_[f].pool, NULL);
            if (frames_[f].fence) vkDestroyFence(dev_, frames_[f].fence, NULL);
            if (frames_[f].acquired) vkDestroySemaphore(dev_, frames_[f].acquired, NULL);
            if (frames_[f].rendered) vkDestroySemaphore(dev_, frames_[f].rendered, NULL);
        }
        if (queries_) vkDestroyQueryPool(dev_, queries_, NULL);
        if (tex_pool_) vkDestroyDescriptorPool(dev_, tex_pool_, NULL);
        if (pipe_layout_) vkDestroyPipelineLayout(dev_, pipe_layout_, NULL);
        for (int i = 0; i < 3; ++i)
            if (layouts_[i]) vkDestroyDescriptorSetLayout(dev_, layouts_[i], NULL);
        if (cmd_pool_) vkDestroyCommandPool(dev_, cmd_pool_, NULL);
        if (swapchain_) vkDestroySwapchainKHR(dev_, swapchain_, NULL);
        vkDestroyDevice(dev_, NULL);
    }
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, NULL);
    if (instance_) vkDestroyInstance(instance_, NULL);
    delete[] batch_;
}

/* ---- set-up ---- */

bool Impl::init(int width, int height, bool display, std::string* error) {
    width_ = width;
    height_ = height;
    if (!create_instance() || !pick_device() || !create_device(display) || !create_common()) {
        if (error) *error = "Vulkan set-up failed (see the log)";
        return false;
    }
    display_ = display && create_swapchain();
    if (display && !display_) {
        if (error) *error = "no swapchain";
        return false;
    }
    scene_ = make_target(width, height);
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = 0;
    td.width = td.height = td.orig_width = td.orig_height = 1;
    td.levels.resize(1);
    td.levels[0].assign(4, 0xFF);
    white_ = (VkTexture*)create_texture(td);
    set_gamma(1.0, false);
    vert_ = module("canvas.vert");
    if (!scene_ || !white_ || !ramp_ || !vert_) {
        if (error) *error = "Vulkan targets or shaders failed (see the log)";
        return false;
    }
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys_, &props);
    log_info("vk: %s, %dx%d%s", props.deviceName, width, height, display_ ? "" : " (offscreen)");
    return true;
}

bool Impl::create_instance() {
    std::vector<const char*> exts;
    u32 n = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &n, NULL);
    std::vector<VkExtensionProperties> props(n);
    if (n) vkEnumerateInstanceExtensionProperties(NULL, &n, &props[0]);
    bool surface = false, display = false;
    for (u32 i = 0; i < n; ++i) {
        if (!strcmp(props[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME)) surface = true;
        if (!strcmp(props[i].extensionName, VK_KHR_DISPLAY_EXTENSION_NAME)) display = true;
    }
    if (surface && display) {
        exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        exts.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
    }
    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "spire360";
    app.pEngineName = "spire360";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (u32)exts.size();
    ci.ppEnabledExtensionNames = exts.empty() ? NULL : &exts[0];
#if !defined(__XSDK__)
    /* host checks: SPIRE_VK_VALIDATE=1 turns on Khronos's validation layer */
    const char* layer = "VK_LAYER_KHRONOS_validation";
    const char* validate = getenv("SPIRE_VK_VALIDATE");
    if (validate && validate[0] == '1') {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = &layer;
    }
#endif
    return vk_ok(vkCreateInstance(&ci, NULL, &instance_), "vkCreateInstance");
}

bool Impl::pick_device() {
    u32 n = 0;
    vkEnumeratePhysicalDevices(instance_, &n, NULL);
    if (!n) {
        log_warn("vk: no Vulkan device");
        return false;
    }
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(instance_, &n, &devs[0]);
    phys_ = devs[0];
    vkGetPhysicalDeviceMemoryProperties(phys_, &mem_props_);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys_, &props);
    uniform_align_ = props.limits.minUniformBufferOffsetAlignment;
    if (uniform_align_ < 64) uniform_align_ = 64;
    u32 qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, NULL);
    std::vector<VkQueueFamilyProperties> qf(qn);
    if (qn) vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, &qf[0]);
    for (u32 i = 0; i < qn; ++i)
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_ = i;
            /* 0: no time stamps (no GPU timing) */
            tick_ns_ = qf[i].timestampValidBits ? props.limits.timestampPeriod : 0;
            return true;
        }
    log_warn("vk: no graphics queue");
    return false;
}

bool Impl::create_device(bool display) {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi;
    memset(&qi, 0, sizeof(qi));
    qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = queue_family_;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    std::vector<const char*> exts;
    if (display) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    VkDeviceCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = (u32)exts.size();
    ci.ppEnabledExtensionNames = exts.empty() ? NULL : &exts[0];
    if (!vk_ok(vkCreateDevice(phys_, &ci, NULL, &dev_), "vkCreateDevice")) return false;
    vkGetDeviceQueue(dev_, queue_family_, 0, &queue_);
    return true;
}

bool Impl::create_swapchain() {
    u32 n = 0;
    if (vkGetPhysicalDeviceDisplayPropertiesKHR(phys_, &n, NULL) != VK_SUCCESS || !n) return false;
    std::vector<VkDisplayPropertiesKHR> displays(n);
    vkGetPhysicalDeviceDisplayPropertiesKHR(phys_, &n, &displays[0]);
    u32 m = 0;
    vkGetDisplayModePropertiesKHR(phys_, displays[0].display, &m, NULL);
    if (!m) return false;
    std::vector<VkDisplayModePropertiesKHR> modes(m);
    vkGetDisplayModePropertiesKHR(phys_, displays[0].display, &m, &modes[0]);
    VkDisplaySurfaceCreateInfoKHR sci;
    memset(&sci, 0, sizeof(sci));
    sci.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    sci.displayMode = modes[0].displayMode;
    sci.planeIndex = 0;
    sci.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sci.globalAlpha = 1.0f;
    sci.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    sci.imageExtent = modes[0].parameters.visibleRegion;
    if (!vk_ok(vkCreateDisplayPlaneSurfaceKHR(instance_, &sci, NULL, &surface_), "vkCreateDisplayPlaneSurfaceKHR"))
        return false;
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_, surface_, &caps);
    VkSwapchainCreateInfoKHR ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface_;
    ci.minImageCount = caps.minImageCount > 2 ? caps.minImageCount : 2;
    ci.imageFormat = TARGET_FORMAT;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent.width = (u32)width_;
    ci.imageExtent.height = (u32)height_;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    if (!vk_ok(vkCreateSwapchainKHR(dev_, &ci, NULL, &swapchain_), "vkCreateSwapchainKHR")) return false;
    u32 count = 0;
    vkGetSwapchainImagesKHR(dev_, swapchain_, &count, NULL);
    std::vector<VkImage> images(count);
    vkGetSwapchainImagesKHR(dev_, swapchain_, &count, &images[0]);
    for (u32 i = 0; i < count; ++i) {
        VkTexture* t = new VkTexture;
        t->width = t->orig_width = width_;
        t->height = t->orig_height = height_;
        t->swapchain = true;
        t->image = images[i];
        swap_images_.push_back(t);
        if (!make_view(t, TARGET_FORMAT) || !make_framebuffer(t)) return false;
    }
    return true;
}

bool Impl::create_common() {
    VkCommandPoolCreateInfo pci;
    memset(&pci, 0, sizeof(pci));
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = queue_family_;
    if (!vk_ok(vkCreateCommandPool(dev_, &pci, NULL, &cmd_pool_), "vkCreateCommandPool")) return false;
    if (tick_ns_ > 0) {
        VkQueryPoolCreateInfo qi;
        memset(&qi, 0, sizeof(qi));
        qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qi.queryCount = TRACE_BASE + TRACE_MAX;
        /* without it, no GPU timing */
        if (vkCreateQueryPool(dev_, &qi, NULL, &queries_) != VK_SUCCESS) queries_ = VK_NULL_HANDLE;
    }

    /* set 0: the draw's texture; set 1: the parameters (dynamic offset);
     * set 2: the extra samplers */
    VkDescriptorSetLayoutBinding b[X_COUNT];
    memset(b, 0, sizeof(b));
    for (int i = 0; i < X_COUNT; ++i) {
        b[i].binding = (u32)i;
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo li;
    memset(&li, 0, sizeof(li));
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = b;
    if (!vk_ok(vkCreateDescriptorSetLayout(dev_, &li, NULL, &layouts_[0]), "vkCreateDescriptorSetLayout")) return false;
    VkDescriptorSetLayoutBinding ub;
    memset(&ub, 0, sizeof(ub));
    ub.binding = 0;
    ub.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    ub.descriptorCount = 1;
    ub.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    li.pBindings = &ub;
    if (!vk_ok(vkCreateDescriptorSetLayout(dev_, &li, NULL, &layouts_[1]), "vkCreateDescriptorSetLayout")) return false;
    li.bindingCount = X_COUNT;
    li.pBindings = b;
    if (!vk_ok(vkCreateDescriptorSetLayout(dev_, &li, NULL, &layouts_[2]), "vkCreateDescriptorSetLayout")) return false;
    VkPushConstantRange push;
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = 32;
    VkPipelineLayoutCreateInfo pli;
    memset(&pli, 0, sizeof(pli));
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 3;
    pli.pSetLayouts = layouts_;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &push;
    if (!vk_ok(vkCreatePipelineLayout(dev_, &pli, NULL, &pipe_layout_), "vkCreatePipelineLayout")) return false;

    /* the textures' sets live as long as their textures */
    VkDescriptorPoolSize sizes[2];
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = 8192;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    sizes[1].descriptorCount = 1;
    VkDescriptorPoolCreateInfo dpi;
    memset(&dpi, 0, sizeof(dpi));
    dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpi.maxSets = 8193;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(dev_, &dpi, NULL, &tex_pool_), "vkCreateDescriptorPool")) return false;

    for (int i = 0; i < FRAMES; ++i) {
        Frame& f = frames_[i];
        VkCommandBufferAllocateInfo ai;
        memset(&ai, 0, sizeof(ai));
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = cmd_pool_;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (!vk_ok(vkAllocateCommandBuffers(dev_, &ai, &f.cmd), "vkAllocateCommandBuffers")) return false;
        VkFenceCreateInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(dev_, &fi, NULL, &f.fence);
        VkSemaphoreCreateInfo si;
        memset(&si, 0, sizeof(si));
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(dev_, &si, NULL, &f.acquired);
        vkCreateSemaphore(dev_, &si, NULL, &f.rendered);
        VkDescriptorPoolSize xs;
        xs.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        xs.descriptorCount = EXTRA_SETS * X_COUNT;
        VkDescriptorPoolCreateInfo xpi;
        memset(&xpi, 0, sizeof(xpi));
        xpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        xpi.maxSets = EXTRA_SETS;
        xpi.poolSizeCount = 1;
        xpi.pPoolSizes = &xs;
        if (!vk_ok(vkCreateDescriptorPool(dev_, &xpi, NULL, &f.pool), "vkCreateDescriptorPool")) return false;
    }
    if (!make_buffer(vertex_ring_, VERTEX_RING * FRAMES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false) ||
        !make_buffer(uniform_ring_, UNIFORM_RING * FRAMES + PARAM_BYTES, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false) ||
        !make_buffer(staging_ring_, STAGING_RING * FRAMES, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true))
        return false;

    VkDescriptorSetAllocateInfo sai;
    memset(&sai, 0, sizeof(sai));
    sai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sai.descriptorPool = tex_pool_;
    sai.descriptorSetCount = 1;
    sai.pSetLayouts = &layouts_[1];
    if (!vk_ok(vkAllocateDescriptorSets(dev_, &sai, &uniform_set_), "vkAllocateDescriptorSets")) return false;
    VkDescriptorBufferInfo bi;
    bi.buffer = uniform_ring_.buf;
    bi.offset = 0;
    bi.range = PARAM_BYTES;
    VkWriteDescriptorSet w;
    memset(&w, 0, sizeof(w));
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = uniform_set_;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    w.pBufferInfo = &bi;
    vkUpdateDescriptorSets(dev_, 1, &w, 0, NULL);

    for (int i = 0; i < 4; ++i) {
        VkSamplerCreateInfo s;
        memset(&s, 0, sizeof(s));
        s.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        s.magFilter = s.minFilter = (i & 2) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        s.addressModeU = s.addressModeV = s.addressModeW =
            (i & 1) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.maxLod = 0.0f;
        if (!vk_ok(vkCreateSampler(dev_, &s, NULL, &samplers_[i]), "vkCreateSampler")) return false;
    }
    compat_pass_ = render_pass(VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    return compat_pass_ != VK_NULL_HANDLE;
}

/* A type with every wanted flag and none of avoid's, else one with the
 * wanted flags, else any. */
u32 Impl::memory_type(u32 bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid) {
    for (u32 i = 0; i < mem_props_.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mem_props_.memoryTypes[i].propertyFlags & want) == want &&
            !(mem_props_.memoryTypes[i].propertyFlags & avoid))
            return i;
    for (u32 i = 0; i < mem_props_.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mem_props_.memoryTypes[i].propertyFlags & want) == want) return i;
    for (u32 i = 0; i < mem_props_.memoryTypeCount; ++i)
        if (bits & (1u << i)) return i;
    return 0;
}

/* cached: the CPU's cached memory where the device has it (vulkan4xenon
 * gives it to buffers only copied from and to, which copies read on the
 * CPU: write-combined memory reads back at ~40 MB/s). */
bool Impl::make_buffer(Buffer& b, VkDeviceSize size, VkBufferUsageFlags usage, bool cached) {
    VkBufferCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!vk_ok(vkCreateBuffer(dev_, &ci, NULL, &b.buf), "vkCreateBuffer")) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(dev_, b.buf, &req);
    VkMemoryAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    ai.memoryTypeIndex = cached ? memory_type(req.memoryTypeBits, want | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0)
                                : memory_type(req.memoryTypeBits, want, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (!vk_ok(vkAllocateMemory(dev_, &ai, NULL, &b.mem), "vkAllocateMemory (buffer)")) {
        vkDestroyBuffer(dev_, b.buf, NULL);
        b = Buffer();
        return false;
    }
    vkBindBufferMemory(dev_, b.buf, b.mem, 0);
    void* p = NULL;
    if (!vk_ok(vkMapMemory(dev_, b.mem, 0, VK_WHOLE_SIZE, 0, &p), "vkMapMemory")) return false;
    b.map = (u8*)p;
    b.size = size;
    return true;
}

void Impl::destroy_buffer(Buffer& b) {
    if (b.buf) vkDestroyBuffer(dev_, b.buf, NULL);
    if (b.mem) vkFreeMemory(dev_, b.mem, NULL);
    b = Buffer();
}

bool Impl::make_image(VkTexture* t, VkFormat format, VkImageUsageFlags usage) {
    VkImageCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent.width = (u32)t->width;
    ci.extent.height = (u32)t->height;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(dev_, &ci, NULL, &t->image), "vkCreateImage")) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev_, t->image, &req);
    VkMemoryAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    if (!vk_ok(vkAllocateMemory(dev_, &ai, NULL, &t->mem), "vkAllocateMemory (image)")) {
        vkDestroyImage(dev_, t->image, NULL);
        t->image = VK_NULL_HANDLE;
        return false;
    }
    t->memory_bytes = (size_t)req.size;
    vkBindImageMemory(dev_, t->image, t->mem, 0);
    return make_view(t, format);
}

bool Impl::make_view(VkTexture* t, VkFormat format) {
    VkImageViewCreateInfo vi;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = t->image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vk_ok(vkCreateImageView(dev_, &vi, NULL, &t->view), "vkCreateImageView");
}

bool Impl::make_framebuffer(VkTexture* t) {
    VkFramebufferCreateInfo fi;
    memset(&fi, 0, sizeof(fi));
    fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fi.renderPass = compat_pass_;
    fi.attachmentCount = 1;
    fi.pAttachments = &t->view;
    fi.width = (u32)t->width;
    fi.height = (u32)t->height;
    fi.layers = 1;
    return vk_ok(vkCreateFramebuffer(dev_, &fi, NULL, &t->fb), "vkCreateFramebuffer");
}

/* A target that is sampled afterwards (and the scene read back) */
VkTexture* Impl::make_target(int w, int h) {
    VkTexture* t = new VkTexture;
    t->width = t->orig_width = w;
    t->height = t->orig_height = h;
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (!make_image(t, TARGET_FORMAT, usage) || !make_framebuffer(t)) {
        log_warn("vk: no %dx%d target", w, h);
        free_texture(t);
        return NULL;
    }
    return t;
}

void Impl::free_texture(VkTexture* t) {
    for (int i = 0; i < 4; ++i)
        if (t->sets[i]) vkFreeDescriptorSets(dev_, tex_pool_, 1, &t->sets[i]);
    if (t->fb) vkDestroyFramebuffer(dev_, t->fb, NULL);
    if (t->view) vkDestroyImageView(dev_, t->view, NULL);
    if (!t->swapchain) {
        if (t->image) vkDestroyImage(dev_, t->image, NULL);
        if (t->mem) vkFreeMemory(dev_, t->mem, NULL);
    }
    delete t;
}

VkRenderPass Impl::render_pass(VkAttachmentLoadOp load, VkImageLayout initial, VkImageLayout final) {
    u32 key = (u32)load | ((u32)initial << 4) | ((u32)final << 16);
    std::map<u32, VkRenderPass>::iterator it = passes_.find(key);
    if (it != passes_.end()) return it->second;
    VkAttachmentDescription a;
    memset(&a, 0, sizeof(a));
    a.format = TARGET_FORMAT;
    a.samples = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp = load;
    a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a.initialLayout = initial;
    a.finalLayout = final;
    VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub;
    memset(&sub, 0, sizeof(sub));
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkSubpassDependency deps[2];
    memset(deps, 0, sizeof(deps));
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_TRANSFER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &a;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 2;
    ci.pDependencies = deps;
    VkRenderPass rp = VK_NULL_HANDLE;
    vk_ok(vkCreateRenderPass(dev_, &ci, NULL, &rp), "vkCreateRenderPass");
    passes_[key] = rp;
    return rp;
}

/* The build's SPIR-V by its file's name (vulkan4xenon finds each in the
 * bundle the build made of them). */
VkShaderModule Impl::module(const char* file) {
    for (size_t i = 0; i < sizeof(SPIRE_VK_SHADERS) / sizeof(SPIRE_VK_SHADERS[0]); ++i) {
        if (strcmp(SPIRE_VK_SHADERS[i].name, file) != 0) continue;
        VkShaderModuleCreateInfo ci;
        memset(&ci, 0, sizeof(ci));
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = SPIRE_VK_SHADERS[i].bytes;
        ci.pCode = SPIRE_VK_SHADERS[i].code;
        VkShaderModule m = VK_NULL_HANDLE;
        if (!vk_ok(vkCreateShaderModule(dev_, &ci, NULL, &m), file)) return VK_NULL_HANDLE;
        return m;
    }
    log_warn("vk: no shader %s in the build", file);
    return VK_NULL_HANDLE;
}

VkPipeline Impl::pipeline(int v, int blend) {
    if (pipes_[v][blend]) return pipes_[v][blend];
    if (pipe_failed_[v][blend] || frag_failed_[v]) return VK_NULL_HANDLE;
    if (!frag_[v]) {
        std::string file = v >= V_EFFECT ? format("effect_%d.frag", v - V_EFFECT) : std::string(VARIANT_FILES[v]);
        frag_[v] = module(file.c_str());
        if (!frag_[v]) {
            frag_failed_[v] = true;
            ++failed_;
            return VK_NULL_HANDLE;
        }
    }
    VkPipelineShaderStageCreateInfo st[2];
    memset(st, 0, sizeof(st));
    st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vert_;
    st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = frag_[v];
    st[1].pName = "main";
    /* xost's inputs by location: 0 position, 1 colour, 2 the texture's
     * coordinates (the Vertex: position, uv, colour) */
    VkVertexInputBindingDescription vb = { 0, (u32)sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription va[3] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 },
                                                { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },
                                                { 2, 0, VK_FORMAT_R32G32_SFLOAT, 8 } };
    VkPipelineVertexInputStateCreateInfo vi;
    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = va;
    VkPipelineInputAssemblyStateCreateInfo ia;
    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vps;
    memset(&vps, 0, sizeof(vps));
    vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1;
    vps.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs;
    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms;
    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba;
    memset(&ba, 0, sizeof(ba));
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT;
    ba.blendEnable = blend != BLEND_NONE ? VK_TRUE : VK_FALSE;
    ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_ADD;
    ba.srcColorBlendFactor = ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    switch (blend) {
    case BLEND_ADD:
        ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case BLEND_SUB:
        ba.colorBlendOp = ba.alphaBlendOp = VK_BLEND_OP_REVERSE_SUBTRACT;
        ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        break;
    case BLEND_MUL:
        /* D3DBLEND_DESTCOLOR: the alpha by the target's alpha */
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        break;
    default:
        ba.dstColorBlendFactor = ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    }
    VkPipelineColorBlendStateCreateInfo cb;
    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds;
    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;
    VkGraphicsPipelineCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount = 2;
    ci.pStages = st;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vps;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &ds;
    ci.layout = pipe_layout_;
    ci.renderPass = compat_pass_;
    VkPipeline p = VK_NULL_HANDLE;
    if (!vk_ok(vkCreateGraphicsPipelines(dev_, VK_NULL_HANDLE, 1, &ci, NULL, &p), "vkCreateGraphicsPipelines")) {
        log_warn("vk: no pipeline for shader %d, blend %d", v, blend);
        pipe_failed_[v][blend] = true;
        ++failed_;
        return VK_NULL_HANDLE;
    }
    pipes_[v][blend] = p;
    return p;
}

/* ---- frames ---- */

void Impl::begin_frame() {
    if (frame_started_) return;
    Frame& f = frames_[frame_];
    double w0 = now_ms();
    vkWaitForFences(dev_, 1, &f.fence, VK_TRUE, ~(u64)0);
    stats_.fence_ms += now_ms() - w0;
    vkResetFences(dev_, 1, &f.fence);
    if (f.timed) {
        /* the frame this slot held last: its first command to its last */
        u64 t[2];
        if (vkGetQueryPoolResults(dev_, queries_, (u32)frame_ * 2, 2, sizeof(t), t, sizeof(u64),
                                  VK_QUERY_RESULT_64_BIT) == VK_SUCCESS &&
            t[1] >= t[0]) {
            last_gpu_ms_ = (double)(t[1] - t[0]) * tick_ns_ / 1e6;
            ++stats_.frames;
            stats_.gpu_ms += last_gpu_ms_;
            if (last_gpu_ms_ > stats_.gpu_worst) stats_.gpu_worst = last_gpu_ms_;
        }
        f.timed = false;
    }
    release_garbage(f);
    vertex_used_ = uniform_used_ = staging_used_ = 0;
    begin_commands();
    frame_started_ = true;
    if (timing_) {
        vkCmdResetQueryPool(f.cmd, queries_, (u32)frame_ * 2, 2);
        vkCmdWriteTimestamp(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries_, (u32)frame_ * 2);
    }
}

void Impl::begin_commands() {
    Frame& f = frames_[frame_];
    vkResetCommandBuffer(f.cmd, 0);
    vkResetDescriptorPool(dev_, f.pool, 0);
    VkCommandBufferBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &bi);
    VkDeviceSize vo = (VkDeviceSize)frame_ * VERTEX_RING;
    vkCmdBindVertexBuffers(f.cmd, 0, 1, &vertex_ring_.buf, &vo);
    bound_pipe_ = VK_NULL_HANDLE;
    bound_tex_set_ = bound_extra_set_ = VK_NULL_HANDLE;
    params_valid_ = false;
    extra_set_ = VK_NULL_HANDLE;
    extra_sets_ = 0;
    dirty_ = true;
}

void Impl::release_garbage(Frame& f) {
    for (size_t i = 0; i < f.spare.size(); ++i) destroy_buffer(f.spare[i]);
    f.spare.clear();
    for (size_t i = 0; i < f.dead.size(); ++i) free_texture(f.dead[i]);
    f.dead.clear();
}

/* Submits what's recorded; wait: waits for it and goes on recording the
 * same frame (full rings, read-backs). */
void Impl::submit(bool wait) {
    Frame& f = frames_[frame_];
    vkEndCommandBuffer(f.cmd);
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    vk_ok(vkQueueSubmit(queue_, 1, &si, f.fence), "vkQueueSubmit");
    if (!wait) return;
    ++stats_.waits;
    vkWaitForFences(dev_, 1, &f.fence, VK_TRUE, ~(u64)0);
    vkResetFences(dev_, 1, &f.fence);
    release_garbage(f);
    vertex_used_ = uniform_used_ = staging_used_ = 0;
    begin_commands();
    /* the GPU idle meanwhile */
    mark(MARK_RESUME, 0, 0, 0, NULL);
}

void Impl::flush_and_wait() {
    begin_frame();
    bool reopen = pass_open_;
    VkTexture* t = pass_tex_;
    end_pass();
    submit(true);
    if (reopen) open_pass(t, false, NULL, NULL);
}

/* ---- passes ---- */

/* A render pass over t (clear: to clear_color; fresh: whatever; else what
 * t holds), its area the whole target unless given. */
void Impl::open_pass(VkTexture* t, bool clear, const float* clear_color, const VkRect2D* area, bool fresh) {
    begin_frame();
    VkAttachmentLoadOp load = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                              : fresh || t->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                                : VK_ATTACHMENT_LOAD_OP_LOAD;
    VkImageLayout final = t->swapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkRenderPass rp = render_pass(load, t->layout, final);
    if (trace_on())
        log_info("vk trace: pass %s %dx%d %s%s", t == scene_ ? "scene" : t == group_ ? "group" : t->swapchain ? "display" : "target",
                 t->width, t->height, load == VK_ATTACHMENT_LOAD_OP_CLEAR ? "clear" : load == VK_ATTACHMENT_LOAD_OP_LOAD ? "load" : "fresh",
                 area ? " (an area)" : "");
    VkClearValue cv;
    memset(&cv, 0, sizeof(cv));
    if (clear_color) memcpy(cv.color.float32, clear_color, sizeof(float) * 4);
    VkRenderPassBeginInfo bi;
    memset(&bi, 0, sizeof(bi));
    bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bi.renderPass = rp;
    bi.framebuffer = t->fb;
    if (area) {
        bi.renderArea = *area;
    } else {
        bi.renderArea.extent.width = (u32)t->width;
        bi.renderArea.extent.height = (u32)t->height;
    }
    bi.clearValueCount = 1;
    bi.pClearValues = &cv;
    VkCommandBuffer cmd = frames_[frame_].cmd;
    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
    t->layout = final; /* once the pass ends */
    pass_tex_ = t;
    pass_open_ = true;
    VkViewport vp = { 0, 0, (float)t->width, (float)t->height, 0, 1 };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc;
    sc.offset.x = sc.offset.y = 0;
    sc.extent.width = (u32)t->width;
    sc.extent.height = (u32)t->height;
    vkCmdSetScissor(cmd, 0, 1, &sc);
    set_target_constants(t->width, t->height);
    bound_pipe_ = VK_NULL_HANDLE;
    bound_tex_set_ = bound_extra_set_ = VK_NULL_HANDLE;
    dirty_ = true;
    ++stats_.passes;
    /* its begin's clear or draw-back */
    mark(MARK_BEGIN, target_kind(t), load == VK_ATTACHMENT_LOAD_OP_CLEAR ? 0 : load == VK_ATTACHMENT_LOAD_OP_LOAD ? 1 : 2,
         0, t);
}

/* The pass of the target the draws go to, begun if it isn't */
void Impl::ensure_pass() {
    if (pass_open_ && pass_tex_ == target_) return;
    end_pass();
    static const float transparent[4] = { 0, 0, 0, 0 };
    const VkRect2D* area = target_area_.extent.width ? &target_area_ : NULL;
    if (target_ == scene_) {
        open_pass(scene_, scene_clear_, clear_, NULL);
        scene_clear_ = false;
    } else if (target_ == group_ && target_clear_) {
        /* A group's pass begins with whatever eDRAM holds there, cleared in
         * the pass. Begun with a clear, a pass of the scene's size has
         * vulkan4xenon resolve the scene with that clear as the scene's pass
         * ends and put the group over the scene's tiles, the scene drawn
         * back into eDRAM after it: two copies of the screen a group, 11 a
         * frame in the Underdocks (tools/xsdk/edram_model.py). So the
         * scene's resolve waits in eDRAM beside the group. */
        open_pass(group_, false, NULL, area, true);
        VkClearAttachment ca;
        memset(&ca, 0, sizeof(ca));
        ca.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkClearRect cr;
        cr.rect.offset.x = cr.rect.offset.y = 0;
        cr.rect.extent.width = (u32)group_->width;
        cr.rect.extent.height = (u32)group_->height;
        if (area) cr.rect = *area;
        cr.baseArrayLayer = 0;
        cr.layerCount = 1;
        vkCmdClearAttachments(frames_[frame_].cmd, 1, &ca, 1, &cr);
        target_clear_ = false;
    } else {
        open_pass(target_, target_clear_, transparent, area);
        target_clear_ = false;
    }
    memset(&target_area_, 0, sizeof(target_area_));
}

void Impl::end_pass() {
    if (!pass_open_) return;
    vkCmdEndRenderPass(frames_[frame_].cmd);
    /* its resolve */
    mark(MARK_END, target_kind(pass_tex_), 0, 0, NULL);
    pass_open_ = false;
    pass_tex_ = NULL;
}

int Impl::target_kind(const VkTexture* t) const {
    if (t == scene_) return TARGET_SCENE;
    if (t == group_) return TARGET_GROUP;
    if (t && t->swapchain) return TARGET_DISPLAY;
    for (int i = 0; i < SCREEN_LEVELS; ++i)
        if (t == screen_lv_[i]) return TARGET_SCREEN;
    return TARGET_CAPTURE;
}

/* A traced frame's time stamp, once the GPU is idle: what it took since
 * the last is what's recorded between them. */
void Impl::mark(int kind, int variant, int blend, int vertices, const VkTexture* tex) {
    if (!tracing_ || marks_.size() >= (size_t)TRACE_MAX) return;
    vkCmdWriteTimestamp(frames_[frame_].cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries_,
                        (u32)(TRACE_BASE + marks_.size()));
    Mark m;
    m.kind = kind;
    m.variant = variant;
    m.blend = blend;
    m.label = label_;
    m.vertices = vertices;
    m.w = tex ? tex->width : 0;
    m.h = tex ? tex->height : 0;
    marks_.push_back(m);
}

void Impl::trace_label(const char* label) {
    if (!tracing_ || (label_ >= 0 && labels_[label_] == label)) return;
    /* the last item's batch is its own */
    flush();
    labels_.push_back(label);
    label_ = (int)labels_.size() - 1;
}

Impl::LayerCounts Impl::layer_counts() const {
    LayerCounts c;
    memset(&c, 0, sizeof(c));
#if defined(__XSDK__)
    V4xShaderCounts s = v4x_shader_counts();
    V4xEdramCounts e = v4x_edram_counts();
    c.compiled = s.compiled;
    c.read = s.read;
    c.drawn_back = e.drawn_back;
    c.clear_resolves = e.clear_resolves;
    c.put_off = e.resolves_put_off;
    c.late_resolves = e.late_resolves;
#endif
    return c;
}

namespace {
struct Cost {
    double ms;
    int count;
    Cost() : ms(0), count(0) {}
};

bool by_cost(const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
    return a.first > b.first;
}
}

/* The traced frame's costs, once its fence has passed: each time stamp's
 * time since the one before charged to what it follows. */
void Impl::trace_report() {
    tracing_ = false;
    size_t n = marks_.size();
    if (n < 2) return;
    std::vector<u64> t(n);
    if (vkGetQueryPoolResults(dev_, queries_, TRACE_BASE, (u32)n, n * sizeof(u64), &t[0], sizeof(u64),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS) {
        log_warn("vk: the GPU trace's time stamps could not be read");
        return;
    }
    LayerCounts lc = layer_counts();
    std::map<std::string, Cost> items, shaders, passes;
    std::vector<std::pair<double, std::string> > draws;
    double total = 0;
    for (size_t i = 1; i < n; ++i) {
        double ms = t[i] >= t[i - 1] ? (double)(t[i] - t[i - 1]) * tick_ns_ / 1e6 : 0;
        total += ms;
        const Mark& m = marks_[i];
        std::string what;
        if (m.kind == MARK_DRAW || m.kind == MARK_QUAD) {
            std::string shader = m.variant >= V_EFFECT ? format("effect %d", m.variant - V_EFFECT)
                                 : m.variant >= 0 && m.variant < V_EFFECT ? std::string(VARIANT_FILES[m.variant])
                                                                          : std::string("?");
            if (shader.size() > 5 && shader.compare(shader.size() - 5, 5, ".frag") == 0) shader.erase(shader.size() - 5);
            shader += format(" blend %d", m.blend);
            Cost& s = shaders[shader];
            s.ms += ms;
            ++s.count;
            std::string item = m.kind == MARK_QUAD ? "(a pass's quad: " + shader + ")"
                               : m.label >= 0      ? labels_[m.label]
                                                   : std::string("(no item)");
            Cost& c = items[item];
            c.ms += ms;
            ++c.count;
            draws.push_back(std::make_pair(ms, format("%s, %d vertices, texture %dx%d: ", shader.c_str(), m.vertices,
                                                      m.w, m.h) + item));
            continue;
        }
        if (m.kind == MARK_BEGIN)
            what = format("%s begin (%s)", TARGET_NAMES[m.variant], LOAD_NAMES[m.blend]);
        else if (m.kind == MARK_END)
            what = format("%s end", TARGET_NAMES[m.variant]);
        else if (m.kind == MARK_UPLOAD)
            what = "texture uploads";
        else
            what = "idle while the CPU waited";
        Cost& p = passes[what];
        p.ms += ms;
        ++p.count;
    }
    log_info("gpu trace (%s): %.1f ms over %u time stamps, each after the GPU finished what came before; "
             "eDRAM: %u drawn back, %u clear resolves, %u resolves put off (%u later)",
             trace_why_.c_str(), total, (unsigned)n, lc.drawn_back - trace_counts_.drawn_back,
             lc.clear_resolves - trace_counts_.clear_resolves, lc.put_off - trace_counts_.put_off,
             lc.late_resolves - trace_counts_.late_resolves);
    std::map<std::string, Cost>* groups[3] = { &items, &shaders, &passes };
    const char* heads[3] = { "item", "shader", "pass" };
    const size_t shown[3] = { 12, 8, 8 };
    for (int g = 0; g < 3; ++g) {
        std::vector<std::pair<double, std::string> > order;
        for (std::map<std::string, Cost>::const_iterator it = groups[g]->begin(); it != groups[g]->end(); ++it)
            order.push_back(std::make_pair(it->second.ms, it->first));
        std::sort(order.begin(), order.end(), by_cost);
        for (size_t i = 0; i < order.size() && i < shown[g]; ++i)
            log_info("gpu trace %s: %.2f ms (%d): %s", heads[g], order[i].first, (*groups[g])[order[i].second].count,
                     order[i].second.c_str());
    }
    std::sort(draws.begin(), draws.end(), by_cost);
    for (size_t i = 0; i < draws.size() && i < 5; ++i)
        log_info("gpu trace draw: %.2f ms %s", draws[i].first, draws[i].second.c_str());
}

void Impl::barrier(VkTexture* t, VkImageLayout to) {
    if (t->layout == to) return;
    VkImageMemoryBarrier b;
    memset(&b, 0, sizeof(b));
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask = to == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL   ? VK_ACCESS_TRANSFER_WRITE_BIT
                      : to == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_ACCESS_TRANSFER_READ_BIT
                                                                   : VK_ACCESS_SHADER_READ_BIT;
    b.oldLayout = t->layout;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = t->image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    VkPipelineStageFlags stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    vkCmdPipelineBarrier(frames_[frame_].cmd, stages, stages, 0, 0, NULL, 0, NULL, 1, &b);
    t->layout = to;
}

/* The vertex shader's push constants: screen pixels of a w x h target to
 * Vulkan's clip space (y down, pixel centres on halves: the software
 * renderer's rules, which the XDK build met with its half-pixel offset). */
void Impl::set_target_constants(int w, int h) {
    float pc[8] = { 2.0f / w, 2.0f / h, -1.0f, -1.0f, 1.0f / w, 1.0f / h, 0, 0 };
    vkCmdPushConstants(frames_[frame_].cmd, pipe_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
    target_w_ = w;
    target_h_ = h;
}

/* A quad over the whole of the open pass's target from tex, through a
 * variant (the copy, level and present passes). The parameters must be
 * set (params_). */
void Impl::draw_quad(VkTexture* tex, int variant, int blend, int sampler, int w, int h) {
    VkCommandBuffer cmd = frames_[frame_].cmd;
    VkPipeline p = pipeline(variant, blend);
    if (!p) return;
    if (!room(6)) {
        flush_and_wait();
        cmd = frames_[frame_].cmd;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
    bound_pipe_ = p;
    float fw = (float)w, fh = (float)h;
    Vertex q[6];
    float px[6] = { 0, fw, fw, 0, fw, 0 }, py[6] = { 0, 0, fh, 0, fh, fh };
    for (int i = 0; i < 6; ++i) q[i] = Vertex(Vec2(px[i], py[i]), Vec2(px[i] / fw, py[i] / fh), Color(1, 1, 1, 1));
    uniform_used_ = align_up(uniform_used_, uniform_align_);
    u32 offset = (u32)((VkDeviceSize)frame_ * UNIFORM_RING + uniform_used_);
    gpu_copy(uniform_ring_.map + offset, params_, PARAM_BYTES);
    uniform_used_ += PARAM_BYTES;
    params_valid_ = false;
    VkTexture* x[X_COUNT];
    int xs[X_COUNT];
    for (int i = 0; i < X_COUNT; ++i) {
        x[i] = white_;
        xs[i] = 0;
    }
    x[X_LUT] = ramp_;
    xs[X_LUT] = 2;
    VkDescriptorSet sets[3] = { texture_set(tex, sampler), uniform_set_, extra_set(x, xs) };
    if (!sets[0] || !sets[2]) return;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout_, 0, 3, sets, 1, &offset);
    bound_tex_set_ = bound_extra_set_ = VK_NULL_HANDLE;
    u32 first = (u32)(vertex_used_ / sizeof(Vertex));
    gpu_copy(vertex_ring_.map + (VkDeviceSize)frame_ * VERTEX_RING + vertex_used_, q, sizeof(q));
    vertex_used_ += sizeof(q);
    vkCmdDraw(cmd, 6, 1, first, 0);
    ++draws_;
    dirty_ = true;
    mark(MARK_QUAD, variant, blend, 6, tex);
    if (trace_on()) log_info("vk trace: quad shader %d from a %dx%d texture", variant, tex ? tex->width : 0, tex ? tex->height : 0);
}

/* ---- textures ---- */

static VkFormat vk_format(int f) {
    switch (f) {
    case TEX_DXT1: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case TEX_DXT5: return VK_FORMAT_BC3_UNORM_BLOCK;
    default: return VK_FORMAT_B8G8R8A8_UNORM; /* ARGB8: B, G, R, A bytes, as the pack holds them */
    }
}

Texture* Impl::create_texture(const TextureData& d) {
    if (!dev_ || d.levels.empty() || d.width <= 0 || d.height <= 0) return NULL;
    VkTexture* t = new VkTexture;
    t->width = d.width;
    t->height = d.height;
    t->orig_width = d.orig_width;
    t->orig_height = d.orig_height;
    t->format = d.format;
    bool block = d.format != TEX_ARGB8;
    int rows = block ? (d.height + 3) / 4 : d.height;
    int pitch = block ? ((d.width + 3) / 4) * (d.format == TEX_DXT1 ? 8 : 16) : d.width * 4;
    size_t bytes = (size_t)rows * (size_t)pitch;
    /* one level, as the XDK build (its packed mip tail held small ones
     * where the GPU did not read them) */
    if (d.levels[0].size() < bytes ||
        !make_image(t, vk_format(d.format), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        static int logged = 0;
        if (logged < 20) {
            ++logged;
            log_warn("vk: texture %dx%d format %d not created", d.width, d.height, d.format);
        }
        free_texture(t);
        return NULL;
    }
    if (!upload(t, &d.levels[0][0], bytes, d.width, d.height, vk_format(d.format))) {
        destroy_texture(t);
        return NULL;
    }
    return t;
}

/* The texels into the image: through the staging ring, copied by the
 * replay at the submit. Copies run outside render passes, so an open one
 * ends and the next draw loads it back. */
bool Impl::upload(VkTexture* t, const u8* data, size_t bytes, int w, int h, VkFormat format) {
    (void)format;
    VkDeviceSize need = align_up((VkDeviceSize)bytes, 256);
    begin_frame();
    VkBuffer src = staging_ring_.buf;
    VkDeviceSize offset = 0;
    u8* dst = NULL;
    if (need > STAGING_RING / 2) {
        Buffer big;
        if (!make_buffer(big, need, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true)) return false;
        frames_[frame_].spare.push_back(big);
        src = big.buf;
        dst = big.map;
    } else {
        if (align_up(staging_used_, 4096) + need > STAGING_RING) flush_and_wait();
        staging_used_ = align_up(staging_used_, 4096);
        offset = (VkDeviceSize)frame_ * STAGING_RING + staging_used_;
        dst = staging_ring_.map + offset;
        staging_used_ += need;
    }
    memcpy(dst, data, bytes);
    bool reopen = pass_open_;
    VkTexture* open = pass_tex_;
    end_pass();
    barrier(t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy region;
    memset(&region, 0, sizeof(region));
    region.bufferOffset = offset;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = (u32)w;
    region.imageExtent.height = (u32)h;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(frames_[frame_].cmd, src, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier(t, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    mark(MARK_UPLOAD, 0, 0, 0, t);
    ++stats_.uploads;
    stats_.upload_mb += (double)bytes / 1048576.0;
    if (reopen) open_pass(open, false, NULL, NULL);
    return true;
}

void Impl::destroy_texture(Texture* tex) {
    VkTexture* t = (VkTexture*)tex;
    if (!t) return;
    flush();
    if (tex_ == t) tex_ = NULL;
    if (gm_ == t) gm_ = NULL;
    if (vfx_on_ && vfx_.lut == tex) {
        vfx_.lut = NULL;
        vfx_on_ = false;
    }
    if (screen_on_ && (screen_.noise == tex || screen_.mask == tex)) screen_on_ = false;
    for (int k = 0; effect_on_ && k < 5; ++k)
        if (effect_.tex[k] == tex) effect_on_ = false;
    if (fire_on_ && (fire_.noise1 == tex || fire_.noise2 == tex || fire_.noise_mask == tex || fire_.dist1 == tex ||
                     fire_.dist2 == tex || fire_.dist_mask == tex))
        fire_on_ = false;
    dirty_ = true;
    extra_set_ = VK_NULL_HANDLE;
    bound_tex_set_ = bound_extra_set_ = VK_NULL_HANDLE;
    /* freed once the GPU is done with the frame being recorded */
    begin_frame();
    frames_[frame_].dead.push_back(t);
}

VkDescriptorSet Impl::texture_set(VkTexture* t, int sampler) {
    if (!t) t = white_;
    if (t->sets[sampler]) return t->sets[sampler];
    VkDescriptorSetAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = tex_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &layouts_[0];
    if (!vk_ok(vkAllocateDescriptorSets(dev_, &ai, &t->sets[sampler]), "vkAllocateDescriptorSets (texture)"))
        return VK_NULL_HANDLE;
    VkDescriptorImageInfo ii;
    ii.sampler = samplers_[sampler];
    ii.imageView = t->view;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet w;
    memset(&w, 0, sizeof(w));
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = t->sets[sampler];
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(dev_, 1, &w, 0, NULL);
    return t->sets[sampler];
}

/* The extra samplers' set, from the frame's pool */
VkDescriptorSet Impl::extra_set(VkTexture* const* tex, const int* samplers) {
    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo ai;
    memset(&ai, 0, sizeof(ai));
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = frames_[frame_].pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &layouts_[2];
    /* room() made sure the pool has one */
    if (!vk_ok(vkAllocateDescriptorSets(dev_, &ai, &set), "vkAllocateDescriptorSets (samplers)")) return VK_NULL_HANDLE;
    ++extra_sets_;
    VkDescriptorImageInfo ii[X_COUNT];
    VkWriteDescriptorSet w[X_COUNT];
    memset(w, 0, sizeof(w));
    for (int i = 0; i < X_COUNT; ++i) {
        VkTexture* t = tex[i] && tex[i]->view ? tex[i] : white_;
        ii[i].sampler = samplers_[samplers[i]];
        ii[i].imageView = t->view;
        ii[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = set;
        w[i].dstBinding = (u32)i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].pImageInfo = &ii[i];
    }
    vkUpdateDescriptorSets(dev_, X_COUNT, w, 0, NULL);
    return set;
}

/* ---- the frame ---- */

void Impl::begin(int width, int height, const Color& c) {
    if (width != width_ || height != height_) {
        /* the scene keeps the size init gave it */
        log_warn("vk: begin %dx%d on a %dx%d renderer", width, height, width_, height_);
    }
    begin_frame();
    if (trace_pending_) {
        /* from here (a frame an upload began is traced from its begin) */
        end_pass();
        trace_pending_ = false;
        tracing_ = true;
        marks_.clear();
        labels_.clear();
        label_ = -1;
        trace_counts_ = layer_counts();
        vkCmdResetQueryPool(frames_[frame_].cmd, queries_, TRACE_BASE, TRACE_MAX);
        mark(MARK_START, 0, 0, 0, NULL);
    }
    screen_levels_ = 0;
    draws_ = 0;
    frame_ended_ = false;
    target_ = scene_;
    target_clear_ = false;
    scene_clear_ = true;
    clear_[0] = c.r;
    clear_[1] = c.g;
    clear_[2] = c.b;
    clear_[3] = c.a;
    memset(&target_area_, 0, sizeof(target_area_));
    clip_on_ = false;
    in_group_ = false;
    cap_w_ = cap_h_ = 0;
    dirty_ = true;
    batch_count_ = 0;
}

void Impl::end() {
    flush();
    if (target_ == scene_) ensure_pass(); /* a frame that drew nothing: the clear */
    end_pass();
    tex_ = NULL;
    frame_ended_ = true;
}

void Impl::present() {
    if (!frame_ended_) end();
    Frame& f = frames_[frame_];
    if (!display_) {
        submit(false);
        if (tracing_) {
            vkWaitForFences(dev_, 1, &f.fence, VK_TRUE, ~(u64)0);
            trace_report();
        }
        frame_started_ = false;
        frame_ = (frame_ + 1) % FRAMES;
        return;
    }
    double a0 = now_ms();
    VkResult r = vkAcquireNextImageKHR(dev_, swapchain_, ~(u64)0, f.acquired, VK_NULL_HANDLE, &image_index_);
    stats_.acquire_ms += now_ms() - a0;
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        vk_ok(r, "vkAcquireNextImageKHR");
        submit(false);
        tracing_ = false;
        frame_started_ = false;
        frame_ = (frame_ + 1) % FRAMES;
        return;
    }
    VkTexture* img = swap_images_[image_index_];
    img->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    /* the scene through the display ramp */
    open_pass(img, false, NULL, NULL);
    memset(params_, 0, sizeof(params_));
    draw_quad(scene_, V_PRESENT, BLEND_NONE, 2, width_, height_);
    end_pass();
    /* a traced frame's time isn't its own: the GPU idles at each stamp */
    if (timing_ && !tracing_) {
        vkCmdWriteTimestamp(f.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries_, (u32)frame_ * 2 + 1);
        f.timed = true;
    }
    vkEndCommandBuffer(f.cmd);
    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.acquired;
    si.pWaitDstStageMask = &stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &f.rendered;
    vk_ok(vkQueueSubmit(queue_, 1, &si, f.fence), "vkQueueSubmit");
#if defined(__XSDK__)
    /* vulkan4xenon's swapchain shows each frame for one vblank; at 30 fps
     * a frame is queued two vblanks after the last one at the soonest */
    XsdkPresentStats st;
    xsdk_present_stats(&st);
    if (interval_ >= 2) {
        double start = now_ms();
        while (st.vblanks < last_vblank_ + 2 && now_ms() - start < 50.0) {
            struct timespec ts = { 0, 1000000 };
            nanosleep(&ts, NULL);
            xsdk_present_stats(&st);
        }
        stats_.pace_ms += now_ms() - start;
    }
    last_vblank_ = st.vblanks;
#endif
    VkPresentInfoKHR pi;
    memset(&pi, 0, sizeof(pi));
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &f.rendered;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_;
    pi.pImageIndices = &image_index_;
    vk_ok(vkQueuePresentKHR(queue_, &pi), "vkQueuePresentKHR");
    if (tracing_) {
        vkWaitForFences(dev_, 1, &f.fence, VK_TRUE, ~(u64)0);
        trace_report();
    }
    frame_started_ = false;
    frame_ = (frame_ + 1) % FRAMES;
}

void Impl::finish() {
    if (!frame_ended_) end();
    begin_frame();
    submit(false);
    vkWaitForFences(dev_, 1, &frames_[frame_].fence, VK_TRUE, ~(u64)0);
    if (tracing_) trace_report();
    frame_started_ = false;
    frame_ = (frame_ + 1) % FRAMES;
}

void Impl::wait_idle() {
    if (frame_started_) {
        /* what's recorded so far first */
        flush_and_wait();
    }
    vkQueueWaitIdle(queue_);
}

/* The display ramp as a 256x1 texture the present pass reads each channel
 * through. */
void Impl::set_gamma(double exponent, bool limited) {
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = 0;
    td.width = td.orig_width = 256;
    td.height = td.orig_height = 1;
    td.levels.resize(1);
    td.levels[0].resize(256 * 4);
    for (int i = 0; i < 256; ++i) {
        double v = pow(i / 255.0, exponent);
        if (limited) v = (16.0 + v * 219.0) / 255.0;
        int b = (int)(v * 255.0 + 0.5);
        if (b > 255) b = 255;
        u8* p = &td.levels[0][(size_t)i * 4];
        p[0] = p[1] = p[2] = (u8)b;
        p[3] = 255;
    }
    VkTexture* t = (VkTexture*)create_texture(td);
    if (!t) return;
    if (ramp_) destroy_texture(ramp_);
    ramp_ = t;
}

/* ---- canvas groups ---- */

bool Impl::begin_group(const Rect2& b) {
    if (group_debug_ == 1 || in_group_ || cap_w_ || group_failed_) return false;
    if (!group_) {
        group_ = make_target(width_, height_);
        if (!group_) {
            log_warn("vk: no canvas group target; clip_children draws unmasked");
            group_failed_ = true;
            return false;
        }
    }
    flush();
    /* the bounds in whole pixels, grown to 32 (vulkan4xenon clears and
     * resolves render areas on its 8-pixel grid) */
    int x0 = (int)floorf(b.pos.x), y0 = (int)floorf(b.pos.y);
    int x1 = (int)ceilf(b.pos.x + b.size.x), y1 = (int)ceilf(b.pos.y + b.size.y);
    x0 &= ~31;
    y0 &= ~31;
    x1 = (x1 + 31) & ~31;
    y1 = (y1 + 31) & ~31;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width_) x1 = width_;
    if (y1 > height_) y1 = height_;
    if (x1 <= x0 || y1 <= y0) {
        x0 = y0 = 0;
        x1 = y1 = 32;
    }
    end_pass();
    target_ = group_;
    target_clear_ = true;
    target_area_.offset.x = x0;
    target_area_.offset.y = y0;
    target_area_.extent.width = (u32)(x1 - x0);
    target_area_.extent.height = (u32)(y1 - y0);
    /* debug mode 2: the whole target, as a pass of the scene's */
    if (group_debug_ == 2) memset(&target_area_, 0, sizeof(target_area_));
    in_group_ = true;
    dirty_ = true;
    ++stats_.groups;
    return true;
}

void Impl::end_group() {
    if (!in_group_) return;
    flush();
    if (pass_tex_ == group_) end_pass();
    else if (target_clear_) {
        /* nothing drew into it: the clear */
        ensure_pass();
        end_pass();
    }
    in_group_ = false;
    target_ = scene_;
    target_clear_ = false;
    dirty_ = true;
}

/* ---- captures: a target of their size, its image given to the texture
 * returned ---- */

bool Impl::begin_capture(int w, int h) {
    if (cap_w_ || in_group_ || w <= 0 || h <= 0 || w > width_ || h > height_) return false;
    u32 key = ((u32)w << 16) | (u32)h;
    VkTexture* t = captures_[key];
    if (!t) {
        t = make_target(w, h);
        if (!t) {
            captures_.erase(key);
            return false;
        }
        captures_[key] = t;
    }
    flush();
    end_pass();
    target_ = t;
    target_clear_ = true;
    cap_w_ = w;
    cap_h_ = h;
    dirty_ = true;
    ++stats_.captures;
    return true;
}

static void swap_images(VkTexture* a, VkTexture* b) {
    VkImage i = a->image;
    a->image = b->image;
    b->image = i;
    VkDeviceMemory m = a->mem;
    a->mem = b->mem;
    b->mem = m;
    VkImageView v = a->view;
    a->view = b->view;
    b->view = v;
    VkFramebuffer f = a->fb;
    a->fb = b->fb;
    b->fb = f;
    VkImageLayout l = a->layout;
    a->layout = b->layout;
    b->layout = l;
    for (int k = 0; k < 4; ++k) {
        VkDescriptorSet s = a->sets[k];
        a->sets[k] = b->sets[k];
        b->sets[k] = s;
    }
    size_t bytes = a->memory_bytes;
    a->memory_bytes = b->memory_bytes;
    b->memory_bytes = bytes;
}

Texture* Impl::end_capture(Texture* reuse) {
    if (!cap_w_) return NULL;
    flush();
    int w = cap_w_, h = cap_h_;
    u32 key = ((u32)w << 16) | (u32)h;
    VkTexture* drawn = captures_[key];
    if (pass_tex_ == drawn) end_pass();
    else if (target_clear_) {
        ensure_pass();
        end_pass();
    }
    cap_w_ = cap_h_ = 0;
    target_ = scene_;
    target_clear_ = false;
    dirty_ = true;
    VkTexture* t = (VkTexture*)reuse;
    if (t && (t->width != w || t->height != h || !t->fb)) {
        destroy_texture(t);
        t = NULL;
    }
    if (!t) {
        /* a target of its own for the next capture of this size */
        t = make_target(w, h);
        if (!t) return NULL;
        t->premultiplied = true;
    }
    /* the texture takes the drawn image; the capture target keeps the
     * texture's old one (the GPU replays in order: the frames that sampled
     * it are done with it before the next capture draws into it) */
    swap_images(t, drawn);
    return t;
}

/* ---- the screen copy ---- */

void Impl::release_screen() {
    for (int i = 0; i < SCREEN_LEVELS; ++i) {
        if (screen_lv_[i]) {
            if (dev_) {
                begin_frame();
                frames_[frame_].dead.push_back(screen_lv_[i]);
            }
            screen_lv_[i] = NULL;
        }
    }
    screen_levels_ = 0;
}

void Impl::level_size(int level, int* w, int* h) const {
    int x = width_, y = height_;
    for (int i = 0; i < level; ++i) {
        x = x > 1 ? x >> 1 : 1;
        y = y > 1 ? y >> 1 : 1;
    }
    *w = x;
    *h = y;
}

/* Level 0: the scene as drawn so far, copied by a pass of its own; the
 * scene's pass goes on after it, from its image. */
bool Impl::copy_screen() {
    if (!screen_lv_[0]) screen_lv_[0] = make_target(width_, height_);
    if (!screen_lv_[0]) return false;
    end_pass();
    if (scene_->layout == VK_IMAGE_LAYOUT_UNDEFINED || scene_clear_) {
        /* not drawn into yet: its clear first */
        ensure_pass();
        end_pass();
    }
    open_pass(screen_lv_[0], false, NULL, NULL);
    memset(params_, 0, sizeof(params_));
    draw_quad(scene_, V_COPY, BLEND_NONE, 2, width_, height_);
    end_pass();
    screen_levels_ = 1;
    ++stats_.screen_copies;
    return true;
}

/* Level from the one above: canvas.hlsl's level pass (the 7x7 Gaussian of
 * the bilinear reads), blending off. */
bool Impl::screen_pass(int level) {
    int w, h;
    level_size(level, &w, &h);
    if (!screen_lv_[level]) screen_lv_[level] = make_target(w, h);
    if (!screen_lv_[level]) return false;
    end_pass();
    open_pass(screen_lv_[level], false, NULL, NULL);
    memset(params_, 0, sizeof(params_));
    params_[88] = 3.0f;
    params_[94] = 1.0f / w;
    params_[95] = 1.0f / h;
    /* g_scrA: the level above, linear, clamped (set 2's screen slot) */
    VkCommandBuffer cmd = frames_[frame_].cmd;
    VkPipeline p = pipeline(V_SCREEN_LEVEL, BLEND_NONE);
    if (!p) {
        end_pass();
        return false;
    }
    if (!room(6)) {
        flush_and_wait();
        cmd = frames_[frame_].cmd;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
    uniform_used_ = align_up(uniform_used_, uniform_align_);
    u32 offset = (u32)((VkDeviceSize)frame_ * UNIFORM_RING + uniform_used_);
    gpu_copy(uniform_ring_.map + offset, params_, PARAM_BYTES);
    uniform_used_ += PARAM_BYTES;
    VkTexture* x[X_COUNT];
    int xs[X_COUNT];
    for (int i = 0; i < X_COUNT; ++i) {
        x[i] = white_;
        xs[i] = 0;
    }
    x[X_SCREEN_A] = screen_lv_[level - 1];
    VkDescriptorSet sets[3] = { texture_set(white_, 0), uniform_set_, extra_set(x, xs) };
    if (!sets[2]) {
        end_pass();
        return false;
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout_, 0, 3, sets, 1, &offset);
    float fw = (float)w, fh = (float)h;
    Vertex q[6];
    float px[6] = { 0, fw, fw, 0, fw, 0 }, py[6] = { 0, 0, fh, 0, fh, fh };
    for (int i = 0; i < 6; ++i) q[i] = Vertex(Vec2(px[i], py[i]), Vec2(px[i] / fw, py[i] / fh), Color(1, 1, 1, 1));
    u32 first = (u32)(vertex_used_ / sizeof(Vertex));
    gpu_copy(vertex_ring_.map + (VkDeviceSize)frame_ * VERTEX_RING + vertex_used_, q, sizeof(q));
    vertex_used_ += sizeof(q);
    vkCmdDraw(cmd, 6, 1, first, 0);
    ++draws_;
    mark(MARK_QUAD, V_SCREEN_LEVEL, BLEND_NONE, 6, screen_lv_[level - 1]);
    ++stats_.screen_levels;
    end_pass();
    params_valid_ = false;
    bound_pipe_ = VK_NULL_HANDLE;
    bound_tex_set_ = bound_extra_set_ = VK_NULL_HANDLE;
    dirty_ = true;
    screen_levels_ = level + 1;
    return true;
}

bool Impl::set_screen(const ScreenParams* p) {
    if (!p) {
        if (screen_on_) {
            flush();
            screen_on_ = false;
            dirty_ = true;
        }
        return true;
    }
    if (screen_failed_ || !dev_ || target_ != scene_) return false;
    int need = p->mode == SCREEN_DARK || p->mode == SCREEN_GLASS ? (int)ceilf(p->lod) : 0;
    if (need > SCREEN_LEVELS - 1) need = SCREEN_LEVELS - 1;
    if (screen_levels_ <= need) {
        flush();
        if (screen_levels_ == 0 && !copy_screen()) {
            log_warn("vk: no screen copy; screen-reading shaders are not drawn");
            screen_failed_ = true;
            return false;
        }
        while (screen_levels_ <= need)
            if (!screen_pass(screen_levels_)) {
                log_warn("vk: no screen copy level %d; screen-reading shaders are not drawn", screen_levels_);
                screen_failed_ = true;
                return false;
            }
    }
    if (!screen_on_ || memcmp(p, &screen_, sizeof(screen_)) != 0) {
        flush();
        screen_on_ = true;
        screen_ = *p;
        dirty_ = true;
    }
    return true;
}

bool Impl::set_effect(const EffectParams* e) {
    if (!e) {
        if (effect_on_) {
            flush();
            effect_on_ = false;
            dirty_ = true;
        }
        return true;
    }
    if (e->id <= 0 || e->id >= EFFECT_COUNT || frag_failed_[V_EFFECT + e->id]) return false;
    if (!effect_on_ || memcmp(e, &effect_, sizeof(effect_)) != 0) {
        flush();
        effect_on_ = true;
        effect_ = *e;
        dirty_ = true;
    }
    return true;
}

void Impl::set_clip(const Rect2* r) {
    bool on = r != NULL;
    int rc[4] = { 0, 0, 0, 0 };
    if (r) {
        rc[0] = (int)(r->pos.x + 0.5f);
        rc[1] = (int)(r->pos.y + 0.5f);
        rc[2] = (int)(r->pos.x + r->size.x + 0.5f);
        rc[3] = (int)(r->pos.y + r->size.y + 0.5f);
    }
    if (on == clip_on_ && (!on || memcmp(rc, clip_, sizeof(rc)) == 0)) return;
    flush();
    clip_on_ = on;
    memcpy(clip_, rc, sizeof(rc));
    dirty_ = true;
}

void Impl::set_msdf(float px_range, float outline) {
    if (msdf_[0] == px_range && msdf_[1] == outline) return;
    flush();
    msdf_[0] = px_range;
    msdf_[1] = outline;
    dirty_ = true;
}

void Impl::set_color_matrix(const float* m) {
    bool on = m != NULL;
    if (on == cm_on_ && (!on || memcmp(m, cm_, sizeof(cm_)) == 0)) return;
    flush();
    cm_on_ = on;
    if (m) memcpy(cm_, m, sizeof(cm_));
    dirty_ = true;
}

void Impl::set_ripple(const float* p) {
    bool on = p != NULL;
    if (on == ripple_on_ && (!on || memcmp(p, ripple_, sizeof(ripple_)) == 0)) return;
    flush();
    ripple_on_ = on;
    if (p) memcpy(ripple_, p, sizeof(ripple_));
    dirty_ = true;
}

void Impl::set_gradient_map(Texture* lut, bool alpha_from_red) {
    VkTexture* t = (VkTexture*)lut;
    if (t == gm_ && (!t || alpha_from_red == gm_red_)) return;
    flush();
    gm_ = t;
    gm_red_ = alpha_from_red;
    dirty_ = true;
}

void Impl::set_fire(const FireParams* p) {
    if (!p && !fire_on_) return;
    if (p && fire_on_ && memcmp(p, &fire_, sizeof(fire_)) == 0) return;
    flush();
    fire_on_ = p != NULL;
    if (p) fire_ = *p;
    dirty_ = true;
}

void Impl::set_vfx(const VfxParams* p) {
    if (!p && !vfx_on_) return;
    if (p && vfx_on_ && memcmp(p, &vfx_, sizeof(vfx_)) == 0) return;
    flush();
    vfx_on_ = p != NULL;
    if (p) vfx_ = *p;
    dirty_ = true;
}

void Impl::draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags) {
    while (count > 0) {
        int n = count < BATCH_MAX ? count : BATCH_MAX;
        memcpy(reserve(tex, n, blend, flags), v, (size_t)n * sizeof(Vertex));
        v += n;
        count -= n;
    }
}

Vertex* Impl::reserve(Texture* tex, int count, BlendMode blend, int flags) {
    VkTexture* t = (VkTexture*)tex;
    if (t != tex_ || blend != blend_ || flags != flags_) {
        flush();
        tex_ = t;
        blend_ = blend;
        flags_ = flags;
        dirty_ = true;
    }
    if (batch_count_ + count > BATCH_MAX) flush();
    Vertex* v = batch_ + batch_count_;
    batch_count_ += count;
    return v;
}

/* ---- draws ---- */

/* canvas.hlsl's choice: an effect's own shader, else ps_main by its
 * branches (the screen, the fire, the erosion, the rest), its lean
 * variants with no mode on. */
int Impl::variant() const {
    if (effect_on_) return V_EFFECT + effect_.id;
    if (screen_on_ && screen_levels_ > 0) {
        switch (screen_.mode) {
        case SCREEN_AT_UV: return V_SCREEN_AT_UV;
        case SCREEN_CHROMA: return V_SCREEN_CHROMA;
        case SCREEN_DISTORT: return V_SCREEN_DISTORT;
        case SCREEN_DOOM: return V_SCREEN_DOOM;
        case SCREEN_GLASS: return V_SCREEN_GLASS;
        case SCREEN_RADIAL: return V_SCREEN_RADIAL;
        case SCREEN_DARK: return V_SCREEN_DARK;
        default: return V_SCREEN_NOISE;
        }
    }
    if (fire_on_) return fire_.strength[3] > 1.5f ? V_DOOM_BAR : V_FIRE;
    if (vfx_on_) return vfx_.polar > 0.5f ? V_VFX_POLAR : V_VFX;
    bool group = (flags_ & DRAW_GROUP) && group_ && !in_group_ && !cap_w_;
    bool msdf = (flags_ & DRAW_MSDF) != 0;
    /* main.frag's modes: the group's (which leaves the MSDF, the colour
     * matrix and the ripple out, the gradient map's alpha not) or one other
     * alone in its own shader, main.frag for more */
    if (group) return gm_ ? V_MAIN : V_GROUP;
    int modes = (cm_on_ ? 1 : 0) + (ripple_on_ ? 1 : 0) + (gm_ ? 1 : 0);
    if (!modes) return msdf ? V_TEXT : V_PLAIN;
    if (modes > 1 || msdf) return V_MAIN;
    return cm_on_ ? V_COLOR_MATRIX : ripple_on_ ? V_RIPPLE : V_GRADIENT_MAP;
}

/* 1 / (e1 - e0), kept off infinity: smoothstep's scale. */
static float smooth_scale(float e0, float e1) {
    float span = e1 - e0;
    if (fabsf(span) < 1e-6f) span = span < 0 ? -1e-6f : 1e-6f;
    return 1.0f / span;
}

/* fire.frag's parameters, c7 to c17, folded as the shader says: the six
 * textures' coordinates two to a vector, the mask's inversion and the
 * strengths as factors, both smoothsteps as v * k + c, the dark variant's
 * colour as the bright one's form; doom_bar.frag's time at c14.x. */
static void fire_params(const FireParams& f, float* c) {
    if (f.strength[3] > 1.5f) {
        c[56] = f.strength[0];
        return;
    }
    const float* uv[6] = { f.noise1_uv, f.noise2_uv, f.mask_uv, f.dist1_uv, f.dist2_uv, f.dist_mask_uv };
    for (int k = 0; k < 6; k += 2) {
        float* scale = c + (7 + k) * 4;
        float* offset = scale + 4;
        for (int j = 0; j < 2; ++j) {
            scale[j] = uv[k][j];
            scale[2 + j] = uv[k + 1][j];
            offset[j] = uv[k][2 + j];
            offset[2 + j] = uv[k + 1][2 + j];
        }
    }
    /* the mask's v is moved by 0.04 about its scale */
    c[41] += 0.04f * (1.0f - f.mask_uv[1]);
    c[52] = f.strength[0] * f.strength[1];
    c[53] = 1.0f - 2.0f * f.strength[2];
    c[54] = f.strength[2];
    c[56] = c[57] = f.dist_strength[0];
    c[58] = c[59] = f.dist_strength[1];
    bool dark = f.strength[3] > 0.5f;
    float ki = smooth_scale(f.steps[0], f.steps[1]), ka = smooth_scale(f.steps[2], f.steps[3]);
    c[60] = dark ? -ki : ki;
    c[61] = ka;
    c[62] = dark ? (1.0f - f.steps[0]) * ki : -f.steps[0] * ki;
    c[63] = -f.steps[2] * ka;
    for (int j = 0; j < 3; ++j) {
        c[64 + j] = dark ? f.outer[j] * f.inner[j] : f.outer[j];
        c[68 + j] = dark ? f.outer[j] : f.inner[j];
    }
}

/* The state of the batch: the pipeline, the parameters (written into the
 * uniform ring when they change), the textures and the scissor. */
void Impl::apply_state() {
    VkCommandBuffer cmd = frames_[frame_].cmd;
    bool group = (flags_ & DRAW_GROUP) && group_ && !in_group_ && !cap_w_;
    int v = variant();
    VkPipeline p = pipeline(v, blend_ == BLEND_PREMULT_ALPHA ? (int)BLEND_MIX : (int)blend_);
    if (p != bound_pipe_ && p) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
        bound_pipe_ = p;
    }
    /* the D3D9 renderer's constants */
    float c[PARAM_VECS * 4 + 4];
    memset(c, 0, sizeof(c));
    float tw = 1, th = 1;
    if (tex_) {
        tw = (float)tex_->width;
        th = (float)tex_->height;
    }
    /* c1, the MSDF's range and the texture's size, which the text and main
     * variants alone read: fixed for the rest, so a change of texture alone
     * leaves their parameters' block as it was (its write, and the GPU's
     * load of it) */
    bool sized = v == V_TEXT || v == V_MAIN;
    float head[28] = { cm_on_ ? 1.0f : 0.0f, (flags_ & DRAW_MSDF) ? 1.0f : 0.0f, group ? 1.0f : 0.0f,
                       ripple_on_ ? 1.0f : 0.0f, sized ? msdf_[0] : 0.0f, sized ? msdf_[1] : 0.0f,
                       sized ? tw : 1.0f, sized ? th : 1.0f, cm_[0], cm_[1], cm_[2], cm_[3],
                       cm_[4], cm_[5], cm_[6], cm_[7], cm_[8], cm_[9], cm_[10], cm_[11], ripple_[0], ripple_[1],
                       ripple_[2], ripple_[3], gm_ ? 1.0f : 0.0f, gm_red_ ? 1.0f : 0.0f, 0.0f, 0.0f };
    memcpy(c, head, sizeof(head));
    /* c7..c17: the stepped fire */
    if (fire_on_) fire_params(fire_, c);
    /* c19..c21: the erosion particle shaders */
    c[76] = vfx_on_ ? 1.0f : 0.0f;
    c[77] = vfx_on_ ? vfx_.polar : 0.0f;
    c[78] = vfx_on_ && vfx_.lut ? 1.0f : 0.0f;
    for (int j = 0; j < 4; ++j) {
        c[80 + j] = vfx_on_ ? vfx_.erosion[j] : 0.0f;
        c[84 + j] = vfx_on_ ? vfx_.distortion[j] : 0.0f;
    }
    /* the samplers past the texture (set 2) */
    VkTexture* x[X_COUNT];
    int xs[X_COUNT];
    for (int i = 0; i < X_COUNT; ++i) {
        x[i] = white_;
        xs[i] = 0;
    }
    if (group) {
        x[X_GROUP] = group_;
        xs[X_GROUP] = 2;
    }
    VkTexture* lut = gm_ ? gm_ : (vfx_on_ && vfx_.lut ? (VkTexture*)vfx_.lut : NULL);
    if (lut) x[X_LUT] = lut;
    if (fire_on_) {
        Texture* ft[6] = { fire_.noise1, fire_.noise2, fire_.noise_mask, fire_.dist1, fire_.dist2, fire_.dist_mask };
        for (int k = 0; k < 6; ++k) {
            x[X_TEX0 + k] = ft[k] ? (VkTexture*)ft[k] : white_;
            xs[X_TEX0 + k] = (k == 2 || k == 5) ? 0 : 1;
        }
    } else if (effect_on_) {
        for (int k = 0; k < 5; ++k) {
            x[X_TEX0 + k] = effect_.tex[k] ? (VkTexture*)effect_.tex[k] : white_;
            xs[X_TEX0 + k] = (effect_.repeat >> k) & 1;
        }
    }
    /* c22..c23 and the screen's samplers */
    if (screen_on_ && screen_levels_ > 0) {
        int l0 = 0;
        float f = 0;
        if (screen_.mode == SCREEN_DARK || screen_.mode == SCREEN_GLASS) {
            float lod = screen_.lod < 0 ? 0 : screen_.lod;
            if (lod > (float)(screen_levels_ - 1)) lod = (float)(screen_levels_ - 1);
            l0 = (int)floorf(lod);
            f = lod - (float)l0;
        }
        int l1 = l0 + 1 < screen_levels_ ? l0 + 1 : l0;
        x[X_SCREEN_A] = screen_lv_[l0];
        x[X_SCREEN_B] = screen_lv_[l1];
        if (screen_.noise) x[X_SCREEN_NOISE] = (VkTexture*)screen_.noise;
        if (screen_.mask) x[X_SCREEN_MASK] = (VkTexture*)screen_.mask;
        xs[X_SCREEN_NOISE] = 1;
        c[88] = (float)screen_.mode;
        c[89] = f;
        c[90] = screen_.mix;
        c[91] = screen_.strength;
        c[92] = screen_.noise_offset[0];
        c[93] = screen_.noise_offset[1];
        if (screen_.mode == SCREEN_DOOM) {
            c[89] = screen_.mix;
            c[90] = screen_.strength;
            c[91] = screen_.power;
            memcpy(&c[92], screen_.color, sizeof(float) * 4);
        }
        if (screen_.mode == SCREEN_GLASS) {
            c[90] = screen_.mix;
            c[91] = screen_.strength;
            memcpy(&c[92], screen_.color, sizeof(float) * 4);
        }
        if (screen_.mode == SCREEN_CHROMA) {
            c[89] = c[90] = 0;
            c[91] = screen_.strength;
        }
        if (screen_.mode == SCREEN_DISTORT) {
            c[89] = screen_.count == 1 ? 1.0f : 0.0f;
            c[90] = 0;
            c[91] = screen_.strength;
            c[92] = screen_.noise_offset[0];
            c[93] = screen_.noise_offset[1];
            c[94] = screen_.center[0];
            c[95] = screen_.center[1];
        }
        if (screen_.mode == SCREEN_RADIAL) {
            int n = screen_.count < 1 ? 1 : (screen_.count > 16 ? 16 : screen_.count);
            c[89] = (float)n;
            c[90] = screen_.power;
            c[91] = 0;
            c[92] = screen_.center[0];
            c[93] = screen_.center[1];
            int count = n;
            memcpy(&c[PARAM_VECS * 4], &count, sizeof(count));
        }
    }
    /* c24..c27, c29..: the effect's */
    if (effect_on_) {
        memcpy(&c[96], effect_.p, sizeof(float) * 16);
        if (effect_.extra_count > 0) memcpy(&c[116], effect_.extra, sizeof(float) * 4 * effect_.extra_count);
    }
    /* c28.x: the texture holds premultiplied colours (a capture) */
    c[112] = tex_ && tex_->premultiplied ? 1.0f : 0.0f;
    bool fresh = !params_valid_ || memcmp(c, params_, PARAM_BYTES) != 0;
    if (fresh) {
        uniform_used_ = align_up(uniform_used_, uniform_align_);
        params_offset_ = (u32)((VkDeviceSize)frame_ * UNIFORM_RING + uniform_used_);
        gpu_copy(uniform_ring_.map + params_offset_, c, PARAM_BYTES);
        memcpy(params_, c, PARAM_BYTES);
        uniform_used_ += PARAM_BYTES;
        params_valid_ = true;
    }
    int sampler = ((flags_ & DRAW_REPEAT) ? 1 : 0) | ((flags_ & DRAW_NEAREST) ? 2 : 0);
    VkDescriptorSet ts = texture_set(tex_, sampler);
    /* the extra samplers' set only for the variants that read them */
    bool extras = v != V_PLAIN && v != V_TEXT && v != V_COLOR_MATRIX && v != V_RIPPLE;
    VkDescriptorSet es = extras ? extra_set(x, xs) : bound_extra_set_;
    if (fresh || !bound_tex_set_ || (extras && es != bound_extra_set_)) {
        VkDescriptorSet sets[3] = { ts, uniform_set_, es };
        u32 count = extras && es ? 3 : 2;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout_, 0, count, sets, 1,
                                &params_offset_);
        bound_tex_set_ = ts;
        if (extras) bound_extra_set_ = es;
    } else if (ts != bound_tex_set_) {
        /* the texture alone: the parameters stay bound at their offset */
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_layout_, 0, 1, &ts, 0, NULL);
        bound_tex_set_ = ts;
    }
    VkRect2D sc;
    int tw_px = target_w_, th_px = target_h_;
    int x0 = 0, y0 = 0, x1 = tw_px, y1 = th_px;
    if (clip_on_) {
        x0 = clip_[0] > 0 ? clip_[0] : 0;
        y0 = clip_[1] > 0 ? clip_[1] : 0;
        x1 = clip_[2] < tw_px ? clip_[2] : tw_px;
        y1 = clip_[3] < th_px ? clip_[3] : th_px;
        if (x1 < x0) x1 = x0;
        if (y1 < y0) y1 = y0;
    }
    sc.offset.x = x0;
    sc.offset.y = y0;
    sc.extent.width = (u32)(x1 - x0);
    sc.extent.height = (u32)(y1 - y0);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    dirty_ = false;
}

/* Whether the frame's rings and pool hold a draw of this many vertices
 * (else what's recorded is submitted first: the draw's state is bound
 * after that, never across it). */
bool Impl::room(int vertices) const {
    return vertex_used_ + (VkDeviceSize)vertices * sizeof(Vertex) <= VERTEX_RING &&
           align_up(uniform_used_, uniform_align_) + PARAM_BYTES <= UNIFORM_RING && extra_sets_ + 1 < EXTRA_SETS;
}

void Impl::flush() {
    if (batch_count_ == 0 || !dev_) return;
    double t0 = now_ms();
    ensure_pass();
    VkDeviceSize bytes = (VkDeviceSize)batch_count_ * sizeof(Vertex);
    if (!room(batch_count_)) flush_and_wait();
    if (dirty_) apply_state();
    double t1 = now_ms();
    if (bound_pipe_) {
        u32 first = (u32)(vertex_used_ / sizeof(Vertex));
        gpu_copy(vertex_ring_.map + (VkDeviceSize)frame_ * VERTEX_RING + vertex_used_, batch_, (size_t)bytes);
        vertex_used_ += bytes;
        vkCmdDraw(frames_[frame_].cmd, (u32)batch_count_, 1, first, 0);
        mark(MARK_DRAW, variant(), (int)blend_, batch_count_, tex_);
        if (trace_on())
            log_info("vk trace: draw shader %d, blend %d, %d vertices, texture %dx%d", variant(), (int)blend_, batch_count_,
                     tex_ ? tex_->width : 0, tex_ ? tex_->height : 0);
        ++draws_;
    }
    double t2 = now_ms();
    state_ms_ += t1 - t0;
    submit_ms_ += t2 - t1;
    batch_count_ = 0;
}

/* ---- read-backs ---- */

bool Impl::read_frame(std::vector<u8>& rgba) {
    if (!frame_ended_) end();
    VkDeviceSize bytes = (VkDeviceSize)width_ * height_ * 4;
    if (readback_.size < bytes) {
        destroy_buffer(readback_);
        if (!make_buffer(readback_, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true)) return false;
    }
    begin_frame();
    barrier(scene_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region;
    memset(&region, 0, sizeof(region));
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = (u32)width_;
    region.imageExtent.height = (u32)height_;
    region.imageExtent.depth = 1;
    vkCmdCopyImageToBuffer(frames_[frame_].cmd, scene_->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_.buf, 1,
                           &region);
    barrier(scene_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    flush_and_wait();
    rgba.resize((size_t)bytes);
    memcpy(&rgba[0], readback_.map, (size_t)bytes);
    for (size_t i = 0; i < (size_t)width_ * height_; ++i) { /* BGRA -> RGBA */
        u8 b = rgba[i * 4];
        rgba[i * 4] = rgba[i * 4 + 2];
        rgba[i * 4 + 2] = b;
    }
    return true;
}

static void put_le(FILE* f, unsigned v, int bytes) {
    for (int i = 0; i < bytes; ++i) fputc((int)((v >> (8 * i)) & 0xFF), f);
}

bool Impl::save_screenshot(const char* path) {
    std::vector<u8> rgba;
    if (!read_frame(rgba)) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int w = width_, h = height_;
    unsigned row = (unsigned)w * 3, size = 54 + row * (unsigned)h;
    fputc('B', f);
    fputc('M', f);
    put_le(f, size, 4);
    put_le(f, 0, 4);
    put_le(f, 54, 4);
    put_le(f, 40, 4);
    put_le(f, (unsigned)w, 4);
    put_le(f, (unsigned)h, 4);
    put_le(f, 1, 2);
    put_le(f, 24, 2);
    put_le(f, 0, 4);
    put_le(f, row * (unsigned)h, 4);
    put_le(f, 2835, 4);
    put_le(f, 2835, 4);
    put_le(f, 0, 4);
    put_le(f, 0, 4);
    std::vector<u8> line(row);
    for (int y = h - 1; y >= 0; --y) {
        const u8* src = &rgba[(size_t)y * w * 4];
        for (int x = 0; x < w; ++x) {
            line[x * 3] = src[x * 4 + 2];
            line[x * 3 + 1] = src[x * 4 + 1];
            line[x * 3 + 2] = src[x * 4];
        }
        fwrite(&line[0], 1, row, f);
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

} /* namespace */

} /* namespace spire */
