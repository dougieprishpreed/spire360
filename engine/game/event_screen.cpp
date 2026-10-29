#include "event_screen.h"
#include "top_bar.h"
#include "combat_screen.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"

#include <ctype.h>
#include <string.h>

namespace spire {

/* NEventRoom.SetupLayout waits 0.2 s before the description and options. */
static const float SETUP_WAIT = 0.2f;
/* NEventOptionButton: _hoverScale, _defaultV, _hoverV; StsColors.blueGlow. */
static const float HOVER_SCALE = 1.01f;
static const float DEFAULT_V = 0.9f, HOVER_V = 1.2f;
static const unsigned BLUE_GLOW_RGB = 0x78FFFE;
static const float BLUE_GLOW_A = 0xC0 / 255.0f;
/* StsColors.cream. */
static const unsigned CREAM = 0xFFF6E2;

static Color json_rgba(const Json& c, const Color& fallback) {
    if (c.size() < 3) return fallback;
    return Color((float)c[(size_t)0].num(), (float)c[1].num(), (float)c[2].num(), c.size() > 3 ? (float)c[3].num() : 1.0f);
}

static std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

static CanvasItem* item(Node* n) { return n && n->is_canvas_item() ? (CanvasItem*)n : NULL; }

/* An option's text as NEventOptionButton._Ready writes it (the logic sends
 * it formatted; older logic: its title and raw description). */
static std::string option_text(const Json& o) {
    if (o["text_bbcode"].is_string()) return o["text_bbcode"].str();
    std::string title = o["title"].is_string() ? o["title"].str() : std::string();
    std::string desc = o["description"].is_string() ? o["description"].str() : std::string();
    if (title.empty()) return desc;
    const char* tag = o["is_locked"].b() ? "red" : "gold";
    return format("[%s][b]%s[/b][/%s]\n%s", tag, title.c_str(), tag, desc.c_str());
}

EventScreen::EventScreen(SceneLibrary* lib)
    : lib_(lib), combat_(NULL), focus_(-1), enabled_(false), enable_at_(0), time_(0), sparring_(false), spar_at_(0),
      steps_options_(NULL), root_(NULL), combat_layout_(false), ancient_(false), line_(0), laid_out_(false),
      continued_(false),
      content_container_(NULL), content_(NULL), options_box_(NULL), next_button_(NULL), content_y_(0),
      container_h_(0), banner_(NULL), banner_title_(NULL), banner_epithet_(NULL) {}

EventScreen::~EventScreen() { delete combat_; }

/* SfxCmd.Play / PlayLoop of a path the logic gave (none: nothing). */
static void play_sfx_cue(SceneLibrary* lib, const char* op, const Json& path) {
    if (!path.is_string() || path.str().empty() || !lib->audio()) return;
    Json cue;
    cue.set("t", Json::make_string("sfx"));
    cue.set("op", Json::make_string(op));
    cue.set("path", path);
    lib->audio()->cue(cue);
}

void EventScreen::play_room_steps(const Json& steps) {
    sparring_ = false;
    if (combat_) combat_->play_steps(steps);
}

bool EventScreen::room_playing() const { return combat_ && combat_->busy(); }

CombatScreen* EventScreen::take_combat() {
    CombatScreen* result = combat_;
    combat_ = NULL;
    Node* room = result ? result->root_node() : NULL;
    if (room && room->parent) room->parent->remove_child(room);
    return result;
}

Node* EventScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    options_.clear();
    lines_.clear();
    tweens_.clear();
    tips_.hide();
    focus_ = -1;
    enabled_ = false;
    time_ = 0;
    line_ = 0;
    laid_out_ = false;
    banner_ = NULL;
    ancient_ = d["layout"].str() == "Ancient";

    Control* root = full_rect_control("EventRoom");
    /* event_room.tscn is not in the bundles: a black room behind the layout. */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Background";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 1);
    root->add_child(back);

    bool combat = d["layout"].str() == "Combat";
    root_ = root;
    combat_layout_ = combat;
    Node* layout = ancient_ ? build_ancient(d) : combat ? build_combat(d) : build_default(d);
    if (!layout) {
        if (error) *error = ancient_ ? "no ancient_event_layout.tscn" : combat ? "no combat event visuals/layout" : "no default_event_layout.tscn";
        delete root;
        return NULL;
    }
    root->add_child(layout);
    root->add_child(frame_driver(this));
    /* NAncientEventLayout.InitializeVisuals: the ancient's ambience loops
     * until the room goes (the floor's loops stop with it). */
    if (ancient_) play_sfx_cue(lib_, "loop", d["ancient"]["ambient_bgm"]);
    /* NEventRoom's container; a combat layout's room holds the shake */
    shake_target_ = !combat && layout->is_control() ? (Control*)layout : NULL;
    return root;
}

/* NCombatEventLayout.SetEvent: the encounter behind the description and
 * event options, with the combat hand and creature interaction disabled. */
Node* EventScreen::build_combat(const Json& d) {
    if (!d["combat_visuals"].is_object()) return NULL;
    Node* lay = instance(lib_, "scenes/events/combat_event_layout.tscn");
    Node* box = lay ? lay->find_unique("CombatRoomContainer") : NULL;
    if (!box) { delete lay; return NULL; }
    Node* room = combat_ ? combat_->root_node() : NULL;
    if (!combat_) {
        combat_ = new CombatScreen(lib_);
        combat_->set_visual_only();
        std::string error;
        room = combat_->build(d["combat_visuals"], &error);
    }
    if (!room) { delete combat_; combat_ = NULL; delete lay; return NULL; }
    box->add_child(room);
    /* PunchOff.AfterEventStarted's sparring lasts until I Can Take Them
     * (TakeThem cancels it) or the room is left; the left construct turns
     * back when it ends. */
    sparring_ = false;
    spar_at_ = 0;
    if (d["event_id"].str() == "punch_off")
        for (size_t i = 0; i < d["options"].size(); ++i)
            if (d["options"][i]["text_key"].str().compare(0, 23, "PUNCH_OFF.pages.INITIAL") == 0) sparring_ = true;
    if (d["event_id"].str() == "punch_off") combat_->set_creature_flipped("e1", sparring_);
    /* what the event's own code did to its room (The Architect's attacks
     * and lines) */
    if (d["steps"].size() > 0) combat_->play_steps(d["steps"]);
    Node* desc = lay->find_unique("EventDescription");
    set_label(desc, d["description_bbcode"].is_string() ? d["description_bbcode"].str() : d["description"].str());
    if (desc && desc->type == "RichTextLabel") {
        /* AnimateIn (Fast mode: 0.45 s, in 0.5 s) */
        RichTextLabel* r = (RichTextLabel*)desc;
        bool fast = fast_mode();
        float delay = SETUP_WAIT + (fast ? 0.45f : 0.75f), time = fast ? 0.5f : 1.0f;
        tweens_.tween_from(&r->modulate.a, 0, 1, time, TRANS_LINEAR, EASE_IN_OUT, delay);
        tweens_.tween_from(&r->visible_ratio, 0, 1, time, TRANS_SINE, EASE_OUT, delay);
    }
    hide(lay->find_unique("SharedEventLabel"));
    Node* options = lay->find_unique("OptionsContainer");
    const Json& os = d["options"];
    options_.reserve(os.size());
    for (size_t i = 0; i < os.size(); ++i) add_option(options, os[i], (int)i, false);
    if (!options_.empty()) focus(0);
    enabled_ = true; enable_at_ = SETUP_WAIT;
    /* The event's code made its options once those steps were done (The
     * Architect's SetEventState after PlayCurrentLine's attacks and waits):
     * shown with the steps, a press skipped the rest of them. */
    if (d["steps"].size() > 0 && !sparring_ && options && options->is_canvas_item()) {
        steps_options_ = (CanvasItem*)options;
        steps_options_->modulate.a = 0;
    }
    return lay;
}

/* NEventLayout: portrait, title, description, options. */
Node* EventScreen::build_default(const Json& d) {
    Node* lay = instance(lib_, "scenes/events/default_event_layout.tscn");
    if (!lay) return NULL;
    std::string id = d["event_id"].is_string() ? d["event_id"].str() : std::string();
    if (!id.empty()) {
        /* SetPortrait: the phobia mode portrait when there is one and the
         * mode is on (EventModel.HasPhobiaModePortrait) */
        std::string phobia = "images/events/" + id + "_phobia_mode.png";
        bool alt = client_settings()["phobia_mode"].b() && lib_->ext(phobia) != NULL;
        set_texture(lib_, lay->find_unique("Portrait"), alt ? phobia : "images/events/" + id + ".png");
    }
    /* InitializeVisuals: the event's own VFX (scenes/vfx/events/<id>_vfx)
     * on the portrait at EventModel.VfxOffset; or what a page put there
     * since (RemoveNodesOnPortrait, AddVfxAnchoredToPortrait, SetPortrait) */
    Node* portrait = lay->find_unique("Portrait");
    std::string vfx = id.empty() ? std::string() : "scenes/vfx/events/" + id + "_vfx.tscn";
    Vec2 vfx_at(268, 49);
    if (room_portrait_.is_object()) {
        if (room_portrait_["image"].is_string()) set_texture(lib_, portrait, room_portrait_["image"].str());
        vfx = room_portrait_["vfx"].str();
        vfx_at = Vec2((float)room_portrait_["at"][(size_t)0].num(), (float)room_portrait_["at"][1].num());
    }
    std::string err;
    Node* fx = portrait && !vfx.empty() ? lib_->instantiate(vfx, &err) : NULL;
    if (fx && fx->is_node2d()) {
        ((Node2D*)fx)->position = vfx_at;
        portrait->add_child(fx);
    } else {
        delete fx;
    }

    /* AnimateIn (from SetDescription, after the setup's wait): the title
     * fades in after 0.5 s (in 0.5 s); the description 0.25 s later, typed
     * out in 1 s. Fast mode: 0.2 s, 0.25 s, 0.5 s. */
    bool fast = fast_mode();
    Node* title = lay->find_unique("Title");
    set_label(title, d["title_text"].is_string() ? d["title_text"].str() : d["event_name"].str());
    if (CanvasItem* t = item(title))
        tweens_.tween_from(&t->modulate.a, 0, 1, fast ? 0.25f : 0.5f, TRANS_LINEAR, EASE_IN_OUT, SETUP_WAIT + (fast ? 0.2f : 0.5f));
    Node* desc = lay->find_unique("EventDescription");
    set_label(desc, d["description_bbcode"].is_string() ? d["description_bbcode"].str() : d["description"].str());
    if (desc && desc->type == "RichTextLabel") {
        RichTextLabel* r = (RichTextLabel*)desc;
        float delay = SETUP_WAIT + (fast ? 0.45f : 0.75f), time = fast ? 0.5f : 1.0f;
        tweens_.tween_from(&r->modulate.a, 0, 1, time, TRANS_LINEAR, EASE_IN_OUT, delay);
        r->visible_ratio = 0;
        tweens_.tween_from(&r->visible_ratio, 0, 1, time, TRANS_SINE, EASE_OUT, delay);
    }
    hide(lay->find_unique("SharedEventLabel")); /* single player */

    Node* box = lay->find_unique("OptionsContainer");
    const Json& os = d["options"];
    options_.reserve(os.size()); /* the tweens hold pointers into it */
    for (size_t i = 0; i < os.size(); ++i) add_option(box, os[i], (int)i, false);
    /* SetOptions: the first option takes the focus. */
    if (!options_.empty()) focus(0);
    enabled_ = true;
    enable_at_ = SETUP_WAIT;
    return lay;
}

void EventScreen::add_option(Node* box, const Json& o, int i, bool ancient) {
    if (!box) return;
    Node* n = instance(lib_, ancient ? "scenes/events/ancient_event_option_button.tscn" : "scenes/events/event_option_button.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    Option op;
    op.node = (Control*)n;
    op.index = o["index"].i(i);
    op.locked = o["is_locked"].b();
    op.will_kill = o["will_kill"].b();
    op.v = DEFAULT_V;
    op.slide = op.slid = 0;
    op.tips = o["tips"];
    Node* text = n->find_unique("Text");
    set_label(text, option_text(o));
    Node* outline = n->get_node("Outline");
    op.outline = outline && outline->is_control() ? (Control*)outline : NULL;
    op.image = item(n->get_node("Image"));
    op.kill_glow = item(n->get_node("RedFlash"));
    if (op.image) op.image->modulate = json_rgba(decision_["button_color"], op.image->modulate);
    if (op.locked) {
        /* SetVisuallyLocked. */
        if (CanvasItem* t = item(text)) t->modulate.a = 0.7f;
        if (op.image) {
            op.image->shader_params["h"] = 0.48f;
            op.image->shader_params["s"] = 0.2f;
        }
        op.v = 0.65f;
    } else if (op.will_kill && op.kill_glow) {
        op.kill_glow->modulate.a = 0.5f; /* ShowPersistentKillGlow */
    }
    if (ancient && o["relic"].is_string()) {
        /* The relic an ancient's option gives. */
        Node* icon = n->find_unique("RelicIcon");
        std::string id = lower(relic_icon_base(o, "relic_icon_base", "relic"));
        set_texture(lib_, icon, "images/atlases/relic_atlas.sprites/" + id + ".tres");
        if (icon) set_texture(lib_, icon->find_unique("Outline"), "images/atlases/relic_outline_atlas.sprites/" + id + ".tres");
        set_visible(icon, true);
    }
    hide(n->find_unique("PlayerVoteContainer"));
    hide(n->get_node("PlayerVoteContainer"));
    if (!ancient) {
        /* AnimateIn: after 0.5 s and 0.2 s per option before it, from 60 px
         * to the left (Back Out) while fading in, in 0.5 s (Fast mode:
         * 0.25 s, 0.1 s, 0.25 s). */
        bool fast = fast_mode();
        float delay = SETUP_WAIT + (fast ? 0.25f : 0.5f) + (fast ? 0.1f : 0.2f) * (float)i, time = fast ? 0.25f : 0.5f;
        op.node->modulate.a = 0;
        tweens_.tween_from(&op.node->modulate.a, 0, 1, time, TRANS_LINEAR, EASE_IN_OUT, delay);
        op.slide = -60;
        options_.push_back(op);
        Option& back = options_.back();
        tweens_.tween_from(&back.slide, -60, 0, time, TRANS_BACK, EASE_OUT, delay);
    } else {
        options_.push_back(op);
    }
    box->add_child(n);
}

/* OnFocus / OnUnfocus (locked options take no focus look). */
void EventScreen::focus(int i) {
    if (focus_ >= 0 && focus_ < (int)options_.size() && focus_ != i) {
        Option& old = options_[(size_t)focus_];
        if (!old.locked) {
            tweens_.tween(&old.node->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween_from(&old.v, HOVER_V, DEFAULT_V, 0.5f, TRANS_EXPO, EASE_OUT);
            if (old.outline) tweens_.tween(&old.outline->modulate.a, 0, 0.3f);
        }
    }
    focus_ = i;
    tips_.hide();
    if (i < 0 || i >= (int)options_.size()) return;
    Option& op = options_[(size_t)i];
    if (op.locked) return;
    if (combat_layout_) tips_.show_for_control(lib_, root_, op.node, op.tips);
    else tips_.show_left(lib_, root_, op.node, op.tips);
    op.node->pivot_offset = offset_size(op.node) * 0.5f;
    tweens_.tween(&op.node->scale, Vec2(HOVER_SCALE, HOVER_SCALE), 0.05f);
    if (op.outline) {
        Color g = hex(BLUE_GLOW_RGB);
        tweens_.tween(&op.outline->modulate.r, g.r, 0.05f);
        tweens_.tween(&op.outline->modulate.g, g.g, 0.05f);
        tweens_.tween(&op.outline->modulate.b, g.b, 0.05f);
        tweens_.tween(&op.outline->modulate.a, BLUE_GLOW_A, 0.05f);
    }
    tweens_.kill(&op.v);
    op.v = HOVER_V;
}

/* NAncientEventLayout: the ancient's scene behind, its name banner, the
 * dialogue and then the options in a clipped column that scrolls up. */
Node* EventScreen::build_ancient(const Json& d) {
    Node* lay = instance(lib_, "scenes/events/ancient_event_layout.tscn");
    if (!lay) return NULL;
    std::string id = d["event_id"].is_string() ? d["event_id"].str() : std::string("neow");
    const Json& anc = d["ancient"];
    Node* bg = lay->find_unique("AncientBgContainer");
    Node* scene = instance(lib_, "scenes/events/background_scenes/" + id + ".tscn");
    if (bg && scene) bg->add_child(scene);
    else delete scene;
    /* NAncientBgContainer._Ready (OnWindowChange at 16:9): at _pos169, at
     * _scale169 about its centre (the full screen's) */
    if (bg && bg->is_control()) {
        Control* c = (Control*)bg;
        const Json* pos = bg->get_extra("_pos169");
        const Json* scale = bg->get_extra("_scale169");
        Vec2 at = pos ? Vec2((float)(*pos)["v"][(size_t)0].num(0), (float)(*pos)["v"][(size_t)1].num(40)) : Vec2(0, 40);
        float k = scale ? (float)(*scale)["v"][(size_t)0].num(0.89) : 0.89f;
        c->pivot_offset = Vec2(960, 540);
        c->scale = Vec2(k, k);
        c->offset[SIDE_LEFT] += at.x;
        c->offset[SIDE_RIGHT] += at.x;
        c->offset[SIDE_TOP] += at.y;
        c->offset[SIDE_BOTTOM] += at.y;
    }

    content_container_ = ctl(lay, "ContentContainer");
    content_ = ctl(lay, "Content");
    options_box_ = ctl(lay, "OptionsContainer");
    Control* next_box = ctl(lay, "FakeNextButtonContainer");
    next_button_ = next_box;
    if (!content_container_ || !content_ || !options_box_) {
        delete lay;
        return NULL;
    }
    show_glyph(lib_, lay->find_unique("HotkeyIcon"), "a");
    hide(lay->find_unique("DialogueHitbox"));

    /* The dialogue lines (NAncientDialogueLine), transparent until shown. */
    Node* dialogue_box = lay->find_unique("DialogueContainer");
    std::string character = lower(decision_["player"]["character"].is_string() ? decision_["player"]["character"].str()
                                                                              : std::string("ironclad"));
    Color ancient_color = json_rgba(anc["dialogue_color"], hex(0x28454F));
    Color character_color = json_rgba(anc["character_dialogue_color"], hex(0x28454F));
    const Json& dl = anc["dialogue"];
    for (size_t i = 0; i < dl.size() && dialogue_box; ++i) {
        Node* ln = instance(lib_, "scenes/events/ancient_dialogue_line.tscn");
        if (!ln || !ln->is_control()) {
            delete ln;
            continue;
        }
        set_label(ln->find_unique("Text"), dl[i]["text"].str());
        bool by_ancient = dl[i]["speaker"].str() != "Character";
        Node* icon = ln->find_unique(by_ancient ? "AncientIcon" : "CharacterIcon");
        if (icon) {
            std::string base = by_ancient ? "images/ui/run_history/" + id : "images/ui/top_panel/character_icon_" + character;
            set_texture(lib_, icon->get_node("Icon"), base + ".png");
            set_texture(lib_, icon->get_node("Icon/Outline"), base + "_outline.png");
        }
        Node* tail = ln->find_unique(by_ancient ? "DialogueTailLeft" : "DialogueTailRight");
        set_visible(tail, true);
        if (Control* tc = ctl(ln, "TextContainer"))
            tc->add_theme_constant_override(by_ancient ? "margin_left" : "margin_right", by_ancient ? 48 : 46);
        Color c = by_ancient ? ancient_color : character_color;
        if (CanvasItem* b = item(ln->find_unique("Bubble"))) b->self_modulate = c;
        if (CanvasItem* t = item(tail)) t->self_modulate = c;
        ((Control*)ln)->modulate.a = 0;
        dialogue_box->add_child(ln);
        lines_.push_back((Control*)ln);
    }

    const Json& os = d["options"];
    options_.reserve(os.size()); /* the tweens hold pointers into it */
    for (size_t i = 0; i < os.size(); ++i) add_option(options_box_, os[i], (int)i, true);

    /* _Ready: the column ends at the "next" button until the last line;
     * OnSetupComplete: the content starts below it. */
    container_h_ = lines_.size() > 1 ? 640.0f : 720.0f;
    content_container_->offset[SIDE_BOTTOM] = content_container_->offset[SIDE_TOP] + container_h_;
    content_y_ = container_h_;
    place_content(container_h_);

    /* NAncientNameBanner: the name and epithet, which turn red and fade,
     * then settle small at the bottom left. */
    Node* banner = instance(lib_, "scenes/ui/ancient_name_banner.tscn");
    if (banner && banner->is_control()) {
        banner_ = (Control*)banner;
        banner_title_ = item(banner->find_unique("Title"));
        banner_epithet_ = item(banner->find_unique("Epithet"));
        set_label(banner_title_, upper(anc["title"].is_string() ? anc["title"].str() : d["event_name"].str()));
        set_label(banner_epithet_, anc["epithet"].str());
        lay->add_child(banner);
    } else {
        delete banner;
    }
    enabled_ = false;
    return lay;
}

/* Content's Position.y (anchored to its container's bottom). */
void EventScreen::place_content(float h) {
    float size = content_->size.y > 0 ? content_->size.y : 298.0f;
    content_->offset[SIDE_TOP] = content_y_ - h;
    content_->offset[SIDE_BOTTOM] = content_->offset[SIDE_TOP] + size;
}

/* SetDialogueLineAndAnimate. */
void EventScreen::show_line(int i) {
    line_ = i;
    bool last = i >= (int)lines_.size() - 1;
    const Json& dl = decision_["ancient"]["dialogue"];
    if (next_button_) {
        Node* label = next_button_->find_unique("Label");
        const Json& next = (size_t)i < dl.size() ? dl[(size_t)i]["next"] : Json();
        if (next.is_string()) set_label(label, next.str());
        set_visible(label, next.is_string());
    }
    float num = 0;
    if (i < (int)lines_.size()) {
        Control* ln = lines_[(size_t)i];
        tweens_.tween(&ln->modulate.a, 1, 0.1f);
        num = ln->position.y + ln->size.y;
        Node* icon = ln->find_unique(dl[(size_t)i]["speaker"].str() != "Character" ? "AncientIcon" : "CharacterIcon");
        set_visible(icon, true);
        /* NAncientDialogueLine.PlaySfx */
        play_sfx_cue(lib_, "play", dl[(size_t)i]["sfx"]);
    }
    if (last) {
        set_visible(next_button_, false);
        container_h_ = 720.0f;
        content_container_->offset[SIDE_BOTTOM] = content_container_->offset[SIDE_TOP] + container_h_;
        /* without lines the options are all there is: up to their bottom
         * (the content is taller, they sit at its end) */
        num = lines_.empty() ? options_box_->position.y + options_box_->size.y + 10.0f
                             : num + options_box_->size.y + 10.0f;
        /* The options take the focus 0.8 s in. */
        enable_at_ = time_ + 0.8f;
        enabled_ = true;
    } else {
        set_visible(next_button_, true);
    }
    tweens_.tween(&content_y_, container_h_ - num, 1.0f, TRANS_EXPO, EASE_OUT);
    for (int k = 0; k < i; ++k) {
        tweens_.kill(&lines_[(size_t)k]->modulate.a);
        lines_[(size_t)k]->modulate.a = 0.25f;
    }
}

std::string EventScreen::input(const PadState& pad) {
    if (ancient_ && laid_out_ && line_ < (int)lines_.size() - 1) {
        /* NAncientDialogueHitbox: NButton's ui_click, no hover sound */
        if (pad.pressed & PadState::A) {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
            show_line(line_ + 1);
        }
        return std::string();
    }
    if (!enabled_ || time_ < enable_at_ || options_.empty() || steps_options_) return std::string();
    int n = (int)options_.size();
    /* NEventOptionButton plays NButton's sounds. */
    int was = focus_;
    if (focus_ < 0) {
        if (pad.pressed & (PadState::UP | PadState::DOWN | PadState::A)) focus(0);
        if (focus_ != was && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
        return std::string();
    }
    if (pad.pressed & PadState::UP) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::DOWN) focus((focus_ + 1) % n);
    if (focus_ != was && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if (pad.pressed & PadState::A) {
        const Option& op = options_[(size_t)focus_];
        if (op.locked) return std::string();
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        return protocol_action("choose_option", format("\"option_index\":%d", op.index));
    }
    return std::string();
}

/* One exchange of PunchEachOther: an attack, 0.1 s later the blunt hit and
 * spark on the other construct and its Hit, 1.2 s later the other way. */
static Json spar_steps() {
    static const char* const STEPS =
        "[[{\"t\":\"anim\",\"who\":\"e1\",\"name\":\"Attack\",\"wait\":0},{\"t\":\"wait\",\"ms\":100},"
        "{\"t\":\"vfx\",\"path\":\"vfx/vfx_attack_blunt\",\"who\":\"e2\",\"at\":\"center\"},"
        "{\"t\":\"ui\",\"call\":\"NHitSparkVfx.Create\",\"args\":[\"e2\",false]},"
        "{\"t\":\"anim\",\"who\":\"e2\",\"name\":\"Hit\",\"wait\":0},{\"t\":\"wait\",\"ms\":1200},"
        "{\"t\":\"anim\",\"who\":\"e2\",\"name\":\"Attack\",\"wait\":0},{\"t\":\"wait\",\"ms\":100},"
        "{\"t\":\"vfx\",\"path\":\"vfx/vfx_attack_blunt\",\"who\":\"e1\",\"at\":\"center\"},"
        "{\"t\":\"ui\",\"call\":\"NHitSparkVfx.Create\",\"args\":[\"e1\",false]},"
        "{\"t\":\"anim\",\"who\":\"e1\",\"name\":\"Hit\",\"wait\":0}]]";
    std::string error;
    return Json::parse(STEPS, strlen(STEPS), &error);
}

void EventScreen::frame(double dt) {
    float before = time_;
    time_ += (float)dt;
    tweens_.update(dt);
    tips_.frame();
    if (sparring_ && combat_ && time_ >= spar_at_) {
        static const Json steps = spar_steps();
        combat_->play_steps(steps);
        spar_at_ += 2.6f;
    }
    if (steps_options_ && !room_playing()) {
        tweens_.tween(&steps_options_->modulate.a, 1, 0.25f);
        enable_at_ = time_ + 0.25f;
        steps_options_ = NULL;
    }
    for (size_t i = 0; i < options_.size(); ++i) {
        Option& op = options_[i];
        /* The slide moves the button's contents (its own place is the
         * container's). */
        float delta = op.slide - op.slid;
        if (delta != 0) {
            for (size_t k = 0; k < op.node->children.size(); ++k) {
                Node* c = op.node->children[k];
                if (!c->is_control()) continue;
                ((Control*)c)->offset[SIDE_LEFT] += delta;
                ((Control*)c)->offset[SIDE_RIGHT] += delta;
            }
            op.slid = op.slide;
        }
        if (op.image) op.image->shader_params["v"] = op.v;
    }
    if (!ancient_) return;
    /* The first line once a layout pass has measured the lines (and the
     * options, which a page without lines rises by alone). */
    if (!laid_out_ && content_->size.y > 0 && (lines_.empty() || lines_[0]->size.y > 0) &&
        (!lines_.empty() || options_.empty() || options_box_->size.y > 0)) {
        laid_out_ = true;
        if (lines_.empty()) {
            container_h_ = 720.0f;
            content_container_->offset[SIDE_BOTTOM] = content_container_->offset[SIDE_TOP] + container_h_;
            content_y_ = container_h_;
        }
        show_line(0);
    }
    place_content(container_h_);
    if (enabled_ && focus_ < 0 && time_ >= enable_at_ && !options_.empty() && !steps_options_) focus(0);

    /* The name banner's timeline (AnimateVfx), simplified: the glyph
     * spacing effect is a fade in. */
    if (banner_) {
        Control* ep = banner_epithet_ && banner_epithet_->is_control() ? (Control*)banner_epithet_ : NULL;
        if (before == 0 && continued_) {
            settle_banner(false);
        } else if (before == 0) {
            banner_->offset[SIDE_TOP] = banner_->offset[SIDE_BOTTOM] = -200;
            tweens_.tween(&banner_->offset[SIDE_TOP], -100, 4.0f, TRANS_CIRC, EASE_OUT);
            tweens_.tween(&banner_->offset[SIDE_BOTTOM], -100, 4.0f, TRANS_CIRC, EASE_OUT);
            if (banner_title_) tweens_.tween_from(&banner_title_->modulate.a, 0, 1, 1.0f, TRANS_EXPO, EASE_OUT);
            if (ep) {
                ep->offset[SIDE_TOP] = ep->offset[SIDE_BOTTOM] = 18;
                ep->modulate.a = 0;
                tweens_.tween(&ep->offset[SIDE_TOP], 42, 2.0f, TRANS_CUBIC, EASE_OUT, 1.0f);
                tweens_.tween(&ep->offset[SIDE_BOTTOM], 42, 2.0f, TRANS_CUBIC, EASE_OUT, 1.0f);
                tweens_.tween(&ep->modulate.a, 1, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 1.5f);
            }
        } else if (!continued_ && before < 4.5f && time_ >= 4.5f) {
            /* Red, then gone (one tween per property: these start now). */
            CanvasItem* both[2] = { banner_title_, banner_epithet_ };
            for (int k = 0; k < 2; ++k) {
                if (!both[k]) continue;
                tweens_.tween(&both[k]->modulate.g, 0, 1.0f, TRANS_EXPO, EASE_OUT);
                tweens_.tween(&both[k]->modulate.b, 0, 1.0f, TRANS_EXPO, EASE_OUT);
                tweens_.tween(&both[k]->modulate.a, 0, 1.0f, TRANS_CUBIC, EASE_OUT);
            }
        } else if (!continued_ && before < 5.5f && time_ >= 5.5f) {
            settle_banner(true);
        }
    }
}

/* The name banner settled: small, cream, at the bottom left. */
void EventScreen::settle_banner(bool animate) {
    if (!banner_) return;
    Control* ep = banner_epithet_ && banner_epithet_->is_control() ? (Control*)banner_epithet_ : NULL;
    CanvasItem* both[2] = { banner_title_, banner_epithet_ };
    for (int k = 0; k < 2; ++k) {
        if (!both[k]) continue;
        tweens_.kill(&both[k]->modulate.r);
        tweens_.kill(&both[k]->modulate.g);
        tweens_.kill(&both[k]->modulate.b);
        tweens_.kill(&both[k]->modulate.a);
    }
    tweens_.kill(&banner_->offset[SIDE_TOP]);
    tweens_.kill(&banner_->offset[SIDE_BOTTOM]);
    banner_->offset[SIDE_TOP] = banner_->offset[SIDE_BOTTOM] = -80;
    banner_->offset[SIDE_LEFT] = banner_->offset[SIDE_RIGHT] = 0;
    if (animate) {
        tweens_.tween(&banner_->offset[SIDE_LEFT], 48, 2.0f, TRANS_CIRC, EASE_OUT);
        tweens_.tween(&banner_->offset[SIDE_RIGHT], 48, 2.0f, TRANS_CIRC, EASE_OUT);
    } else {
        banner_->offset[SIDE_LEFT] = banner_->offset[SIDE_RIGHT] = 48;
    }
    if (banner_title_ && banner_title_->type == "RichTextLabel") {
        RichTextLabel* t = (RichTextLabel*)banner_title_;
        t->modulate = Color(1, 1, 1, 1);
        t->h_align = 0;
        t->v_align = 2;
        t->offset[SIDE_TOP] = t->offset[SIDE_BOTTOM] = 0;
        t->add_theme_font_size_override("normal_font_size", 54);
        t->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0));
        t->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0));
        t->add_theme_color_override("default_color", hex(CREAM));
        t->invalidate();
    }
    if (ep && ep->type == "Label") {
        Label* l = (Label*)ep;
        l->modulate = Color(1, 1, 1, 0);
        l->h_align = 0;
        l->v_align = 2;
        l->add_theme_font_size_override("font_size", 18);
        l->add_theme_color_override("font_outline_color", Color(0, 0, 0, 0));
        l->add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0));
        l->add_theme_color_override("font_color", hex(CREAM));
        if (animate) tweens_.tween(&l->modulate.a, 0.5f, 2.0f, TRANS_CIRC, EASE_OUT);
        else l->modulate.a = 0.5f;
    }
}

} /* namespace spire */
