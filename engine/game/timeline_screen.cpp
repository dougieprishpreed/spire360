#include "timeline_screen.h"
#include "top_bar.h"
#include "../audio/game_audio.h"
#include "card_view.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/canvas.h"
#include "../scene/shaders.h"
#include "../scene/spine_sprite.h"
#include "../spine/spine.h"

#include <math.h>

namespace spire {

/* NEpochSlot's outline colours: StsColors.purple, "70A0FF18". */
static const Color HIGHLIGHT(0xEE / 255.0f, 0x82 / 255.0f, 0xEE / 255.0f, 1.0f);
static const Color OUTLINE_DEFAULT(0x70 / 255.0f, 0xA0 / 255.0f, 1.0f, 0x18 / 255.0f);
/* NTimelineScreen.GrowTimelineAndAddEraIcons: the line per era. */
static const float LINE_PER_ERA = 226.0f;

TimelineScreen::TimelineScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), slots_box_(NULL), line_(NULL), moved_(NULL), inspect_(NULL), holder_(NULL),
      backstop_(NULL), reminder_(NULL), tutorial_(NULL), tip_(NULL), unlock_(NULL), queue_at_(0), mode_(LOADING),
      focus_(-1), pan_(0), pan_target_(0), t_(0), unlock_t_(0), inspect_ready_(0.4), info_text_at_(-1), closing_unlock_(false),
      tutorial_done_(false), tutorial_close_at_(-1) {
    paginator_left_[0] = paginator_left_[1] = 0;
}

Node* TimelineScreen::build(std::string* error) {
    tweens_.clear();
    slots_.clear();
    columns_.clear();
    root_ = instance(lib_, "scenes/timeline_screen/timeline_screen.tscn");
    if (!root_) {
        if (error) *error = "no timeline_screen.tscn";
        return NULL;
    }
    slots_box_ = ctl(root_, "EpochSlots");
    line_ = ctl(root_, "Line");
    moved_ = ctl(root_, "WhatsMoved");
    inspect_ = ctl(root_, "EpochInspectScreen");
    for (int i = 0; i < 2; ++i) {
        Control* b = inspect_ ? ctl(inspect_, i ? "NextChapterButton" : "PrevChapterButton") : NULL;
        paginator_left_[i] = b ? b->offset[SIDE_LEFT] : 0;
    }
    holder_ = ctl(root_, "UnlockScreenHolder");
    backstop_ = ctl(root_, "SharedBackstop");
    reminder_ = ctl(root_, "EpochReminderText");
    hide(ctl(root_, "InputBlocker"));
    hide(inspect_);
    hide(backstop_);
    if (line_) line_->custom_minimum_size.x = 0;
    show_glyph(lib_, find_named(root_->get_node("BackButton"), "HotkeyIcon"), "b");
    /* _Ready: the slots fade in over 1 s. */
    Control* slots = ctl(root_, "SlotsContainer");
    if (slots) tweens_.tween(&slots->modulate.a, 1, 1.0f);
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_open");
    root_->add_child(frame_driver(this));
    mode_ = LOADING;
    return root_;
}

/* NEraColumn.Create / Init: the era's column, in era order. */
TimelineScreen::Column* TimelineScreen::column_for(const Json& e, bool animated) {
    int era = e["era_value"].i();
    for (size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i].era == era) return &columns_[i];
    Node* n = instance(lib_, "scenes/timeline_screen/era_column.tscn");
    if (!n || !n->is_control() || !slots_box_) {
        delete n;
        return NULL;
    }
    Column c;
    c.era = era;
    c.node = (Control*)n;
    set_texture(lib_, ctl(n, "Icon"), e["era_icon"].str());
    set_label(ctl(n, "Name"), e["era_name"].str());
    set_label(ctl(n, "Year"), e["era_year"].str());
    /* MoveChild after the columns of earlier eras. */
    size_t at = 0;
    for (size_t i = 0; i < columns_.size(); ++i)
        if (columns_[i].era < era) ++at;
    slots_box_->add_child(n);
    std::vector<Node*>& ch = slots_box_->children;
    ch.pop_back();
    size_t pos = 0, seen = 0;
    for (; pos < ch.size() && seen < at; ++pos)
        if (ch[pos]->is_control()) ++seen;
    ch.insert(ch.begin() + (std::ptrdiff_t)pos, n);
    columns_.push_back(c);
    /* SpawnIcon: in from 0.1 scale (Back Out) and transparent, 0.5 s;
     * the line grows to the new count (2 s, Cubic InOut). */
    Control* icon = ctl(n, "Icon");
    if (icon) {
        icon->modulate.a = 0;
        icon->scale = Vec2(0.1f, 0.1f);
        float d = animated ? 2.0f : 0.0f;
        tweens_.tween(&icon->modulate.a, 1, 0.5f, TRANS_LINEAR, EASE_IN_OUT, d);
        tweens_.tween(&icon->scale, Vec2(1, 1), 0.5f, TRANS_BACK, EASE_OUT, d);
    }
    if (line_) {
        float w = (float)columns_.size() * LINE_PER_ERA;
        if (animated) tweens_.tween(&line_->custom_minimum_size.x, w, 2.0f, TRANS_CUBIC, EASE_IN_OUT);
        else line_->custom_minimum_size.x = w;
    }
    return &columns_.back();
}

/* NEraColumn.SpawnNameAndYear. */
void TimelineScreen::show_name_and_year(Column& c) {
    Control* name = ctl(c.node, "Name");
    Control* year = ctl(c.node, "Year");
    if (!name || name->self_modulate.a > 0) return;
    float nh = name->offset[SIDE_BOTTOM] - name->offset[SIDE_TOP];
    tweens_.tween(&name->self_modulate.a, 1, 1.0f);
    tweens_.tween_from(&name->offset[SIDE_TOP], -36, 28, 1.0f, TRANS_CUBIC, EASE_OUT);
    tweens_.tween_from(&name->offset[SIDE_BOTTOM], -36 + nh, 28 + nh, 1.0f, TRANS_CUBIC, EASE_OUT);
    if (year) {
        tweens_.tween(&year->modulate.a, 1, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 0.5f);
        float yh = year->offset[SIDE_BOTTOM] - year->offset[SIDE_TOP];
        float top = year->offset[SIDE_TOP];
        tweens_.tween_from(&year->offset[SIDE_TOP], top, top + 20, 1.0f, TRANS_CUBIC, EASE_OUT, 0.5f);
        tweens_.tween_from(&year->offset[SIDE_BOTTOM], top + yh, top + 20 + yh, 1.0f, TRANS_CUBIC, EASE_OUT, 0.5f);
    }
}

TimelineScreen::~TimelineScreen() {
    for (std::map<std::string, Blurred*>::iterator it = blurred_.begin(); it != blurred_.end(); ++it) delete it->second;
}

/* NEpochSlot.SetState(NotObtained): the portrait drawn into the slot's
 * 162 x 100 SubViewport (BlurPortrait, with the slot's hsv material: the
 * same per-instance material as the Portrait, so at s 0.25), under its Blur
 * rect (dark_blur at lod 2, its material's mix_percentage); the Portrait
 * shows that viewport (epoch_slot.tscn). */
Texture2DRes* TimelineScreen::blurred_portrait(Slot& s, const std::string& path) {
    std::map<std::string, Blurred*>::iterator it = blurred_.find(path);
    if (it != blurred_.end()) return &it->second->image;
    Resource* r = path.empty() ? NULL : lib_->ext(path);
    if (!r || !r->is_texture()) return NULL;
    Blurred* b = new Blurred();
    b->recipe.source = (Texture2DRes*)r;
    b->recipe.width = 162;
    b->recipe.height = 100;
    b->recipe.cm_on = true;
    Canvas::hsv_matrix(1.0f, 0.25f, 1.0f, b->recipe.cm);
    b->recipe.lod = 2.0f;
    Node* blur = s.node ? find_named(s.node, "Blur") : NULL;
    MaterialRes* m = blur && blur->is_canvas_item() ? ((CanvasItem*)blur)->material : NULL;
    b->recipe.mix = m ? shader_param(m, "mix_percentage", 0.25f) : 0.25f;
    b->recipe.clear = Color(0.3f, 0.3f, 0.3f, 1); /* Godot's default clear colour */
    b->image.path = "derived/epoch_slot_blur/" + path;
    b->image.size_ = Vec2(162, 100);
    b->image.derived = &b->recipe;
    blurred_[path] = b;
    return &b->image;
}

/* NEpochSlot.SetState. */
void TimelineScreen::set_slot_state(Slot& s, const std::string& state) {
    s.state = state;
    if (s.image) s.image->modulate = Color(1, 1, 1, 1);
    set_visible(s.portrait, true);
    set_texture(lib_, s.portrait, s.data["portrait"].str());
    if (state == "NotObtained" && s.portrait && s.portrait->type == "TextureRect") {
        Texture2DRes* t = blurred_portrait(s, s.data["portrait"].str());
        if (t) ((TextureRect*)s.portrait)->texture = t;
    }
    if (s.portrait) {
        float sat = state == "Complete" ? 1.0f : state == "Obtained" ? 0.0f : 0.25f;
        float val = state == "Obtained" ? 0.5f : 1.0f;
        s.portrait->shader_params["s"] = sat;
        s.portrait->shader_params["v"] = val;
    }
    set_visible(s.chains, state == "Obtained");
    s.pulsing = state == "Obtained";
    if (s.outline) s.outline->modulate = state == "Obtained" ? HIGHLIGHT : state == "Complete" ? Color(0, 0, 0, 0) : OUTLINE_DEFAULT;
    set_highlight(s, state == "Obtained");
}

static void free_node(Control*& c) {
    if (!c) return;
    if (c->parent) c->parent->remove_child(c);
    delete c;
    c = NULL;
}

/* NEpochSlot.EnableHighlight / DisableHighlight: an obtained slot's
 * NEpochHighlightVfx behind it (epoch_highlight_vfx.tscn, child 0) and its
 * NEpochOffscreenVfx (epoch_offscreen_vfx.tscn) in the screen's
 * %ReminderVfxHolder, shown at the screen's edge while the slot is off it
 * (TimelineScreen::frame). */
void TimelineScreen::set_highlight(Slot& s, bool on) {
    if (!on) {
        free_node(s.highlight);
        free_node(s.offscreen);
        s.off_shown = false;
        return;
    }
    if (!s.highlight) {
        Node* h = instance(lib_, "scenes/vfx/epoch_highlight_vfx.tscn");
        if (h && h->is_control()) {
            s.highlight = (Control*)h;
            s.node->add_child(h);
            std::vector<Node*>& ch = s.node->children;
            ch.pop_back();
            ch.insert(ch.begin(), h);
        } else {
            delete h;
        }
    }
    Control* holder = root_ ? ctl(root_, "ReminderVfxHolder") : NULL;
    if (!s.offscreen && holder) {
        Node* o = instance(lib_, "scenes/timeline_screen/epoch_offscreen_vfx.tscn");
        if (o && o->is_control()) {
            s.offscreen = (Control*)o;
            s.offscreen->modulate.a = 0;
            holder->add_child(o);
            s.off_shown = false;
        } else {
            delete o;
        }
    }
}

/* NEpochSlot.SpawnSlot: up from 64 px below, fading in; an obtained one's
 * highlight once it has spawned (EnableHighlight after the tween). */
void TimelineScreen::spawn_slot(Slot& s, float delay) {
    if (s.highlight) {
        s.highlight->modulate.a = 0;
        tweens_.tween(&s.highlight->modulate.a, 1, 0.01f, TRANS_LINEAR, EASE_IN_OUT, delay + 0.5f);
    }
    Control* parts[2] = { s.image, s.outline };
    for (int k = 0; k < 2; ++k) {
        Control* c = parts[k];
        if (!c) continue;
        float h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
        float top = c->offset[SIDE_TOP];
        c->offset[SIDE_TOP] = top + 64;
        c->offset[SIDE_BOTTOM] = top + 64 + h;
        tweens_.tween(&c->offset[SIDE_TOP], top, 0.5f, TRANS_BACK, EASE_OUT, delay);
        tweens_.tween(&c->offset[SIDE_BOTTOM], top + h, 0.5f, TRANS_BACK, EASE_OUT, delay);
    }
    if (s.image) {
        s.image->self_modulate.a = 0;
        tweens_.tween(&s.image->self_modulate.a, 1, 0.5f, TRANS_CUBIC, EASE_OUT, delay);
    }
}

void TimelineScreen::add_slot(const Json& e, bool animated, float delay) {
    Column* col = column_for(e, animated);
    if (!col) return;
    Node* n = instance(lib_, "scenes/timeline_screen/epoch_slot.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return;
    }
    Slot s;
    s.id = e["id"].str();
    s.era = e["era_value"].i();
    s.position = e["era_position"].i();
    s.data = e;
    s.node = (Control*)n;
    s.image = ctl(n, "SlotImage");
    s.portrait = ctl(n, "Portrait");
    s.chains = ctl(n, "Chains");
    s.outline = ctl(n, "Outline");
    s.reticle = ctl(n, "SelectionReticle");
    s.pulsing = false;
    s.highlight = s.offscreen = NULL;
    s.off_shown = false;
    hide(ctl(n, "DebugLabel"));
    if (s.reticle) s.reticle->modulate.a = 0;
    n->name = format("Slot%d", s.position);
    /* NEraColumn.AddSlot: above the slots already there (MoveChild 0). */
    col->node->add_child(n);
    std::vector<Node*>& ch = col->node->children;
    ch.pop_back();
    ch.insert(ch.begin(), n);
    set_slot_state(s, e["state"].str());
    spawn_slot(s, delay);
    slots_.push_back(s);
    if (s.state != "NotObtained") show_name_and_year(*col);
}

/* InitScreen / AddEpochSlots: slots for epochs not shown yet, states of
 * the others. */
void TimelineScreen::sync_slots(const Json& slots, bool animated) {
    float delay = animated ? 2.0f : 0.0f;
    for (size_t i = 0; i < slots.size(); ++i) {
        const Json& e = slots[i];
        bool found = false;
        for (size_t k = 0; k < slots_.size(); ++k) {
            if (slots_[k].id != e["id"].str()) continue;
            found = true;
            slots_[k].data = e;
            if (slots_[k].state != e["state"].str()) set_slot_state(slots_[k], e["state"].str());
            for (size_t c = 0; c < columns_.size(); ++c)
                if (columns_[c].era == slots_[k].era && slots_[k].state != "NotObtained") show_name_and_year(columns_[c]);
        }
        if (found) continue;
        /* Rng.Chaotic: up to 0.3 s each; one after another when animated. */
        float d = (float)(chaotic_rand() % 300) / 1000.0f;
        add_slot(e, animated, delay + d);
        if (animated) delay += 0.5f + d;
    }
}

void TimelineScreen::refresh_reminder() {
    if (!reminder_) return;
    bool show = data_["revealable"].i() > 0;
    set_visible(reminder_, show);
    /* NEpochReminderText: plain text (no bbcode), centred by the label. */
    set_label(ctl(reminder_, "ReminderLabel"), data_["reminder_text"].str());
}

void TimelineScreen::set_data(const Json& d) {
    if (d["type"].str() != "timeline") return;
    if (tutorial_ && tutorial_close_at_ >= 0 && unlock_t_ < tutorial_close_at_) {
        held_ = d;
        return;
    }
    if (mode_ == WAITING && d["revealed"].is_object()) {
        /* A reveal: the inspect screen with the unlock, then its screens;
         * the timeline takes the answer's slots after them. */
        data_ = d;
        queue_ = d["unlocks"];
        queue_at_ = 0;
        after_ = d["slots"];
        for (size_t k = 0; k < slots_.size(); ++k)
            if (slots_[k].id == d["revealed"]["id"].str()) set_slot_state(slots_[k], "Complete");
        open_inspect(d["revealed"], true);
        return;
    }
    bool first = mode_ == LOADING;
    data_ = d;
    if (d["first_time"].b() && first && !tutorial_done_) {
        /* FirstTimeLogic: the tutorial (NTimelineTutorial.AnimateTutorial:
         * the text types in over 2 s (Quad Out) as it fades in over 1 s;
         * after 1 s Proceed rises to 920 (Back Out, 0.3 s) and is enabled
         * once the text is in). The back button hides meanwhile. */
        Node* t = instance(lib_, "scenes/timeline_screen/timeline_tutorial.tscn");
        if (t && t->is_control()) {
            tutorial_ = (Control*)t;
            Control* text = ctl(t, "TutorialText");
            set_label(text, "[center]" + d["tutorial_text"].str() + "[/center]");
            if (text && text->type == "RichTextLabel") {
                RichTextLabel* r = (RichTextLabel*)text;
                r->visible_ratio = 0;
                tweens_.tween(&r->visible_ratio, 1, 2.0f, TRANS_QUAD, EASE_OUT);
            }
            if (text) {
                text->modulate.a = 0;
                tweens_.tween(&text->modulate.a, 1, 1.0f);
            }
            Control* ack = ctl(t, "AcknowledgeButton");
            set_label(ctl(t, "Label"), Loc::get("timeline", "ACKNOWLEDGE_BUTTON"));
            show_glyph(lib_, ctl(t, "HotkeyIcon"), "a");
            if (ack) {
                ack->modulate.a = 0;
                tweens_.tween(&ack->modulate.a, 1, 0.3f, TRANS_CUBIC, EASE_OUT, 1.0f);
                float h = ack->offset[SIDE_BOTTOM] - ack->offset[SIDE_TOP];
                tweens_.tween(&ack->offset[SIDE_TOP], 920.0f - 1080.0f, 0.3f, TRANS_BACK, EASE_OUT, 1.0f);
                tweens_.tween(&ack->offset[SIDE_BOTTOM], 920.0f - 1080.0f + h, 0.3f, TRANS_BACK, EASE_OUT, 1.0f);
            }
            hide(root_->get_node("BackButton"));
            root_->add_child(t);
            mode_ = TUTORIAL;
            unlock_t_ = 0;
            hide(reminder_);
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_unlock");
            return;
        }
        delete t;
    }
    bool animated = tutorial_done_ && mode_ == LOADING;
    set_visible(root_->get_node("BackButton"), true);
    if (tutorial_) {
        if (tutorial_->parent) tutorial_->parent->remove_child(tutorial_);
        delete tutorial_;
        tutorial_ = NULL;
    }
    sync_slots(d["slots"], animated);
    refresh_reminder();
    mode_ = BROWSE;
    /* NavigateToRevealableSlot: the obtained slot nearest the start. */
    int target = 0;
    for (size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].state == "Obtained") {
            target = (int)i;
            break;
        }
    if (!slots_.empty()) focus(target);
}

void TimelineScreen::clear_tip() {
    if (tip_) {
        if (tip_->parent) tip_->parent->remove_child(tip_);
        delete tip_;
        tip_ = NULL;
    }
}

/* NEpochSlot.OnFocus / OnUnfocus. */
void TimelineScreen::focus(int i) {
    if (i < 0 || i >= (int)slots_.size()) return;
    clear_tip();
    if (focus_ >= 0 && focus_ < (int)slots_.size() && focus_ != i) {
        Slot& o = slots_[(size_t)focus_];
        tweens_.tween(&o.node->scale, Vec2(1, 1), 0.5f, TRANS_CUBIC, EASE_OUT);
        if (o.reticle) o.reticle->modulate.a = 0;
        if (o.portrait) {
            float sat = o.state == "Complete" ? 1.0f : o.state == "Obtained" ? 0.0f : 0.25f;
            float val = o.state == "Obtained" ? 0.5f : o.state == "Complete" ? 1.0f : 1.1f;
            tweens_.tween(&o.portrait->shader_params["s"], sat, 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&o.portrait->shader_params["v"], val, 0.5f, TRANS_EXPO, EASE_OUT);
        }
        o.pulsing = o.state == "Obtained";
    }
    focus_ = i;
    Slot& s = slots_[(size_t)i];
    s.pulsing = false;
    if (s.reticle) s.reticle->modulate.a = 1;
    s.node->pivot_offset = s.node->size * 0.5f;
    if (s.state != "NotObtained") tweens_.tween(&s.node->scale, Vec2(1.05f, 1.05f), 0.05f);
    if (s.portrait) {
        if (s.state == "Complete") {
            tweens_.tween(&s.portrait->shader_params["s"], 1.1f, 0.05f);
            tweens_.tween(&s.portrait->shader_params["v"], 1.1f, 0.05f);
        } else if (s.state == "Obtained") {
            tweens_.tween(&s.portrait->shader_params["v"], 0.65f, 0.05f);
        } else {
            tweens_.tween(&s.portrait->shader_params["s"], 0.25f, 0.05f);
            tweens_.tween(&s.portrait->shader_params["v"], 1.2f, 0.05f);
        }
    }
    /* NEpochSlot.HoveredSfx: a locked slot's own */
    if (lib_->audio())
        lib_->audio()->ui(s.state != "NotObtained" ? "event:/sfx/ui/timeline/ui_timeline_hover"
                                                   : "event:/sfx/ui/timeline/ui_timeline_hover_locked");
    /* The hover tip beside the slot (left of it past 70% of the screen). */
    tip_ = make_hover_tip(lib_, s.data["title"].str(),
                          s.data[s.state == "Complete" ? "unlock_info_revealed" : "unlock_info"].str());
    if (tip_) {
        root_->add_child(tip_);
        Vec2 at = s.node->global_transform().xform(Vec2(0, 0));
        float half = s.node->size.x * 0.5f + 6.0f;
        float right = at.x + half;
        tip_->set_position(at.x > 1920 * 0.7f ? Vec2(right - half - 360.0f, at.y) : Vec2(right + half, at.y));
    }
    /* NSlotsContainer.LerpToSlot: the focused slot towards the middle. */
    if (s.node->parent) {
        Vec2 at = s.node->global_transform().xform(Vec2(0, 0)) + s.node->size * 0.5f;
        pan_target_ = pan_ + (960.0f - at.x);
    }
}

/* The slot a direction leads to: the next column with the row nearest to
 * the focused one, or the next row of this column. */
int TimelineScreen::neighbour(int dx, int dy) const {
    if (focus_ < 0) return 0;
    const Slot& f = slots_[(size_t)focus_];
    int best = -1;
    if (dy) {
        /* Up: the slot above (a higher era position). */
        for (size_t i = 0; i < slots_.size(); ++i)
            if (slots_[i].era == f.era && slots_[i].position == (dy < 0 ? f.position + 1 : f.position - 1)) best = (int)i;
        return best < 0 ? focus_ : best;
    }
    int col = -1, best_era = 0;
    for (size_t i = 0; i < columns_.size(); ++i) {
        int e = columns_[i].era;
        if ((dx > 0 && e > f.era) || (dx < 0 && e < f.era)) {
            if (col < 0 || (dx > 0 ? e < best_era : e > best_era)) {
                col = (int)i;
                best_era = e;
            }
        }
    }
    if (col < 0) return focus_;
    int dist = 1 << 30;
    for (size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].era != best_era) continue;
        int d = slots_[i].position - f.position;
        if (d < 0) d = -d;
        if (d < dist) {
            dist = d;
            best = (int)i;
        }
    }
    return best < 0 ? focus_ : best;
}

/* NEpochInspectScreen.Open (revealed: UnlockAnimation). */
void TimelineScreen::open_inspect(const Json& e, bool revealed) {
    if (!inspect_) {
        next_unlock();
        return;
    }
    clear_tip();
    mode_ = INSPECT;
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_open_epoch");
    /* NTimelineScreen.ShowBackstopAndHideUi. */
    if (backstop_) {
        set_visible(backstop_, true);
        backstop_->modulate.a = 0;
        tweens_.tween(&backstop_->modulate.a, 0.5f, 0.4f);
    }
    Control* slots = ctl(root_, "SlotsContainer");
    if (slots) tweens_.tween(&slots->modulate.a, 0.1f, 0.4f);
    hide(reminder_);
    set_visible(inspect_, true);
    inspect_->modulate = Color(1, 1, 1, 1);
    Control* portrait = ctl(inspect_, "Portrait");
    set_texture(lib_, portrait, e["real_portrait"].str());
    if (portrait && !portrait->visible) set_visible(portrait, true);
    hide(ctl(inspect_, "PlaceholderLabel"));
    /* the chapter fades in after 0.2 s and the story after 0.4 s (0.5 s) */
    Control* chapter = ctl(inspect_, "ChapterLabel");
    Control* story = ctl(inspect_, "StoryLabel");
    set_label(chapter, e["chapter_text"].str());
    set_label(story, e["story_title"].str());
    if (chapter) {
        chapter->modulate.a = 0;
        tweens_.tween(&chapter->modulate.a, 1, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.2f);
    }
    if (story) {
        story->modulate.a = 0;
        tweens_.tween(&story->modulate.a, 1, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.4f);
    }
    /* the chapter paginators, but at a reveal (UnlockAnimation's HidePaginators) */
    info_text_at_ = -1;
    show_paginators(revealed ? Json() : e, true);
    hide(ctl(inspect_, "EraLabel"));
    Control* fancy = ctl(inspect_, "FancyText");
    set_label(fancy, e["description"].str());
    /* UnlockAnimation: the chains tighten (0.5 s after 0.5 s) and give
     * (NEpochChains, the scene's 1 s); then the portrait flashes and
     * regains its colour (1 s), the story fades in after 0.25 s (2 s) and
     * the button comes after 1 s. Fast mode: halved, the story after 0.1 s.
     * Not revealed: the story and the button after 0.1 s. */
    bool fast = fast_mode();
    float unlocked = fast ? 1.0f : 2.0f, k = fast ? 0.5f : 1.0f;
    float button_at = revealed ? unlocked + k : 0.1f;
    inspect_ready_ = button_at + 0.3;
    Control* info = ctl(inspect_, "UnlockInfo");
    if (info) {
        /* NUnlockInfo.AnimIn: to 0.8 in 1 s; a reveal keeps it hidden
         * (UnlockAnimation's HideImmediately) */
        set_label(ctl(info, "Text"), e["unlock_text"].str());
        tweens_.kill(&info->modulate.a);
        info->modulate.a = 0;
        if (!revealed) tweens_.tween(&info->modulate.a, 0.8f, 1.0f);
    }
    Control* close = ctl(inspect_, "CloseButton");
    set_label(ctl(close, "CloseLabel"), Loc::get("timeline", revealed ? "EPOCH_INSPECT.continueButton" : "EPOCH_INSPECT.closeButton"));
    show_glyph(lib_, ctl(close, "HotkeyIcon"), "a");
    if (close) {
        close->modulate.a = 0;
        tweens_.tween(&close->modulate.a, 1, 0.3f, TRANS_CUBIC, EASE_OUT, button_at);
    }
    Control* chains = ctl(inspect_, "Chains");
    if (fancy) {
        fancy->modulate.a = 0;
        if (revealed) tweens_.tween(&fancy->modulate.a, 1, 2.0f * k, TRANS_LINEAR, EASE_IN_OUT, unlocked + (fast ? 0.1f : 0.25f));
        else tweens_.tween(&fancy->modulate.a, 1, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    }
    if (revealed) {
        /* the chains' shard effects are not ported: they fade as they give */
        if (portrait) {
            portrait->shader_params["s"] = 0;
            portrait->shader_params["v"] = 0.75f;
            tweens_.tween(&portrait->shader_params["s"], 1, k, TRANS_EXPO, EASE_OUT, unlocked);
            tweens_.tween(&portrait->shader_params["v"], 1, k, TRANS_EXPO, EASE_OUT, unlocked);
        }
        if (chains) {
            set_visible(chains, true);
            chains->modulate = Color(1, 1, 1, 1);
            tweens_.tween(&chains->modulate.a, 0, k, TRANS_LINEAR, EASE_IN_OUT, unlocked - k);
        }
        Control* flash = ctl(inspect_, "PortraitFlash");
        if (flash) tweens_.tween_from(&flash->modulate.a, 1, 0, 0.5f * k, TRANS_LINEAR, EASE_IN_OUT, unlocked);
    } else {
        hide(chains);
        if (portrait) {
            portrait->shader_params["s"] = 1;
            portrait->shader_params["v"] = 1;
        }
    }
    unlock_t_ = 0;
}

/* NEpochInspectScreen.RefreshChapterPaginators: an NEpochPaginateButton
 * shows (OnEnable) for a revealed chapter either side of a story's epoch.
 * Open slides them in from 100 px inside (offset_left, Back Out 0.25 s
 * after 0.25 s) as they fade in; the gold arrow's hsv value is 0.9
 * (NGoldArrowButton._Ready). */
void TimelineScreen::show_paginators(const Json& e, bool slide) {
    for (int i = 0; i < 2; ++i) {
        chapters_[i] = e[i ? "next_chapter" : "prev_chapter"].str();
        Control* b = inspect_ ? ctl(inspect_, i ? "NextChapterButton" : "PrevChapterButton") : NULL;
        if (!b) continue;
        bool on = !chapters_[i].empty();
        set_visible(b, on);
        if (!on || !slide) continue;
        Control* arrow = ctl(b, "TextureRect");
        if (arrow) arrow->shader_params["v"] = 0.9f;
        b->modulate.a = 0;
        tweens_.tween(&b->modulate.a, 1, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.25f);
        b->offset[SIDE_LEFT] = paginator_left_[i];
        tweens_.tween_from(&b->offset[SIDE_LEFT], paginator_left_[i] + (i ? -100.0f : 100.0f), paginator_left_[i], 0.25f,
                           TRANS_BACK, EASE_OUT, 0.25f);
    }
}

/* A paginator's hotkey (the pad's left / right): OpenViaPaginator, the
 * chapter's portrait, text and paginators in place; the unlock info fades
 * out (0.1 s) and back in with the chapter's (NUnlockInfo.AnimInViaPaginator). */
void TimelineScreen::paginate(int side) {
    const Json* found = NULL;
    for (size_t i = 0; i < slots_.size() && !found; ++i)
        if (slots_[i].id == chapters_[side]) found = &slots_[i].data;
    if (!found) return;
    const Json e = *found;
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_click");
    /* NGoldArrowButton.OnPress: the arrow's value drops to 0.7 */
    Control* b = ctl(inspect_, side ? "NextChapterButton" : "PrevChapterButton");
    Control* arrow = b ? ctl(b, "TextureRect") : NULL;
    if (arrow) arrow->shader_params["v"] = 0.7f;
    set_texture(lib_, ctl(inspect_, "Portrait"), e["real_portrait"].str());
    Control* chapter = ctl(inspect_, "ChapterLabel");
    Control* story = ctl(inspect_, "StoryLabel");
    set_label(chapter, e["chapter_text"].str());
    set_label(story, e["story_title"].str());
    if (chapter) { tweens_.kill(&chapter->modulate.a); chapter->modulate.a = 1; }
    if (story) { tweens_.kill(&story->modulate.a); story->modulate.a = 1; }
    Control* fancy = ctl(inspect_, "FancyText");
    set_label(fancy, e["description"].str());
    if (fancy) {
        fancy->modulate.a = 0;
        tweens_.tween(&fancy->modulate.a, 1, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    }
    show_paginators(e, false);
    Control* info = ctl(inspect_, "UnlockInfo");
    if (info) {
        tweens_.tween(&info->modulate.a, 0, 0.1f);
        info_text_ = e["unlock_text"].str();
        info_text_at_ = unlock_t_ + 0.1;
    }
}

void TimelineScreen::close_inspect() {
    hide(inspect_);
    info_text_at_ = -1;
    chapters_[0].clear();
    chapters_[1].clear();
    if (queue_at_ < queue_.size()) {
        next_unlock();
        return;
    }
    /* HideBackstopAndShowUi. */
    if (backstop_) tweens_.tween(&backstop_->modulate.a, 0, 0.4f);
    Control* slots = ctl(root_, "SlotsContainer");
    if (slots) tweens_.tween(&slots->modulate.a, 1, 0.4f);
    if (!after_.is_null()) {
        sync_slots(after_, false);
        after_ = Json();
    }
    refresh_reminder();
    mode_ = BROWSE;
    int f = focus_;
    focus_ = -1;
    focus(f < 0 ? 0 : f);
}

/* The next queued unlock screen (NTimelineScreen.OpenQueuedScreen). */
void TimelineScreen::next_unlock() {
    if (queue_at_ >= queue_.size()) {
        queue_ = Json();
        close_inspect();
        return;
    }
    const Json& u = queue_[queue_at_++];
    const std::string& kind = u["kind"].str();
    mode_ = UNLOCKS;
    closing_unlock_ = false;
    unlock_t_ = 0;
    if (kind == "timeline") {
        /* NUnlockTimelineScreen.AnimateExpansion: the new slots join the
         * timeline (the line grows, then they pop up one by one). */
        if (backstop_) tweens_.tween(&backstop_->modulate.a, 0, 0.4f);
        Control* slots = ctl(root_, "SlotsContainer");
        if (slots) tweens_.tween(&slots->modulate.a, 1, 0.4f);
        sync_slots(u["slots"], true);
        unlock_ = NULL;
        return;
    }
    std::string scene = "scenes/timeline_screen/unlock_" +
                        std::string(kind == "cards" ? "cards" : kind == "relics" ? "relics" : kind == "potions" ? "potions" :
                                    kind == "character" ? "character" : "misc") + "_screen.tscn";
    Node* n = instance(lib_, scene);
    if (!n || !n->is_control()) {
        delete n;
        next_unlock();
        return;
    }
    unlock_ = (Control*)n;
    if (kind == "cards" || kind == "relics" || kind == "potions") {
        const char* banner = kind == "cards" ? "UNLOCK_CARDS_BANNER" : kind == "relics" ? "UNLOCK_RELICS_BANNER" : "UNLOCK_POTIONS_BANNER";
        const char* text = kind == "cards" ? "UNLOCK_CARDS" : kind == "relics" ? "UNLOCK_RELICS" : "UNLOCK_POTIONS";
        set_label(ctl(ctl(n, "Banner"), "Label"), Loc::get("timeline", banner));
        set_label(ctl(n, "ExplanationText"), "[center]" + Loc::get("timeline", text) + "[/center]");
        Control* row = ctl(n, kind == "cards" ? "CardRow" : kind == "relics" ? "RelicRow" : "PotionRow");
        const Json& items = u[kind == "cards" ? "cards" : kind == "relics" ? "relics" : "potions"];
        float step = kind == "cards" ? 350.0f : 200.0f;
        float x0 = -(float)(items.size() - 1) * step * 0.5f;
        for (size_t i = 0; row && i < items.size(); ++i) {
            Node* item = NULL;
            if (kind == "cards") {
                item = make_card(lib_, items[i]);
                card_reward_glow(lib_, item, items[i]["rarity"].str());
            } else {
                TextureRect* t = new TextureRect();
                t->name = "Icon";
                std::string id = lower(kind == "relics" ? relic_icon_base(items[i]) : items[i]["id"].str());
                set_texture(lib_, t, kind == "relics" ? "images/atlases/relic_atlas.sprites/" + id + ".tres"
                                                     : "images/atlases/potion_atlas.sprites/" + id + ".tres");
                place(t, -64, -64, 128, 128);
                t->expand_mode = 1;
                t->stretch_mode = 5;
                item = t;
            }
            if (!item) continue;
            Control* holder = full_rect_control("Holder");
            holder->anchor[SIDE_RIGHT] = holder->anchor[SIDE_BOTTOM] = 0;
            holder->anchor[SIDE_LEFT] = holder->anchor[SIDE_TOP] = 0.5f;
            holder->anchor[SIDE_RIGHT] = holder->anchor[SIDE_BOTTOM] = 0.5f;
            holder->add_child(item);
            row->add_child(holder);
            /* Open: each slides out from the middle (0.5 s, Expo Out),
             * from black to white over 1 s. */
            holder->offset[SIDE_LEFT] = holder->offset[SIDE_RIGHT] = 0;
            tweens_.tween(&holder->offset[SIDE_LEFT], x0 + step * (float)i, 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&holder->offset[SIDE_RIGHT], x0 + step * (float)i, 0.5f, TRANS_EXPO, EASE_OUT);
            holder->modulate = Color(0, 0, 0, 1);
            tweens_.tween(&holder->modulate.r, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&holder->modulate.g, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&holder->modulate.b, 1, 1.0f, TRANS_CUBIC, EASE_OUT);
        }
    } else if (kind == "character") {
        set_label(ctl(n, "TopLabel"), u["top_text"].str());
        set_label(ctl(n, "BottomLabel"), u["bottom_text"].str());
        Control* anchor = ctl(n, "SpineAnchor");
        Node* v = anchor ? instance(lib_, "scenes/creature_visuals/" + lower(u["character"].str()) + ".tscn") : NULL;
        if (v) {
            anchor->add_child(v);
            /* idle_loop, then the attack after 0.5 s, then idle_loop. */
            SpineSprite* sp = (SpineSprite*)NULL;
            std::vector<Node*> stack(1, v);
            while (!stack.empty() && !sp) {
                Node* x = stack.back();
                stack.pop_back();
                if (x->type == "SpineSprite") sp = (SpineSprite*)x;
                for (size_t k = 0; k < x->children.size(); ++k) stack.push_back(x->children[k]);
            }
            if (sp && sp->set_animation("idle_loop", true, 0) && sp->skeleton() && sp->animation_state()) {
                const SpAnimation* attack = sp->skeleton()->data()->find_animation("attack");
                const SpAnimation* idle = sp->skeleton()->data()->find_animation("idle_loop");
                if (attack) sp->animation_state()->add_animation(0, attack, false, 0.5f);
                if (idle) sp->animation_state()->add_animation(0, idle, true, 0);
            }
        }
    } else {
        set_label(ctl(n, "Label"), u["text"].str());
    }
    Control* confirm = ctl(n, "ConfirmButton");
    set_label(confirm ? confirm->get_node("Label") : NULL, Loc::get("timeline", "UNLOCK_CONFIRM"));
    show_glyph(lib_, find_named(confirm, "HotkeyIcon"), "a");
    /* NUnlockScreen.Open: fades in over 0.5 s. */
    unlock_->modulate = Color(1, 1, 1, 0);
    tweens_.tween(&unlock_->modulate.a, 1, 0.5f);
    if (holder_) holder_->add_child(n);
    else root_->add_child(n);
    if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_unlock");
}

/* NUnlockScreen.Close: to transparent black over 1 s. */
void TimelineScreen::close_unlock() {
    if (!unlock_ || closing_unlock_) return;
    closing_unlock_ = true;
    unlock_t_ = 0;
    tweens_.tween(&unlock_->modulate.r, 0, 1.0f);
    tweens_.tween(&unlock_->modulate.g, 0, 1.0f);
    tweens_.tween(&unlock_->modulate.b, 0, 1.0f);
    tweens_.tween(&unlock_->modulate.a, 0, 1.0f);
}

TimelineScreen::Action TimelineScreen::input(const PadState& pad, std::string* cmd) {
    switch (mode_) {
    case LOADING:
    case WAITING:
        return NONE;
    case TUTORIAL:
        /* CloseTutorial once Proceed is enabled. */
        if ((pad.pressed & PadState::A) && unlock_t_ >= 2.0) {
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
            *cmd = "{\"cmd\":\"timeline_first_time_done\"}";
            mode_ = LOADING;
            tutorial_done_ = true;
            if (tutorial_) tweens_.tween(&tutorial_->modulate.a, 0, 0.5f);
            tutorial_close_at_ = unlock_t_ + 0.5;
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_open");
        }
        return NONE;
    case INSPECT:
        /* the paginators' hotkeys (MegaInput.left / right) once they show */
        if (unlock_t_ > 0.25 && (pad.pressed & (PadState::LEFT | PadState::RIGHT))) {
            int side = (pad.pressed & PadState::LEFT) ? 0 : 1;
            if (!chapters_[side].empty()) paginate(side);
            return NONE;
        }
        if ((pad.pressed & (PadState::A | PadState::B)) && unlock_t_ > inspect_ready_) {
            /* NCloseButton.ClickedSfx */
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_close_epoch");
            close_inspect();
        }
        return NONE;
    case UNLOCKS:
        if (unlock_ && !closing_unlock_ && unlock_t_ > 0.5 && (pad.pressed & PadState::A)) {
            /* NAcknowledgeButton.ClickedSfx */
            if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_close_epoch");
            close_unlock();
        }
        return NONE;
    case BROWSE:
        break;
    }
    if (pad.pressed & PadState::B) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/map/map_close");
        clear_tip();
        return BACK;
    }
    if (slots_.empty()) return NONE;
    if (pad.pressed & PadState::LEFT) focus(neighbour(-1, 0));
    if (pad.pressed & PadState::RIGHT) focus(neighbour(1, 0));
    if (pad.pressed & PadState::UP) focus(neighbour(0, -1));
    if (pad.pressed & PadState::DOWN) focus(neighbour(0, 1));
    if ((pad.pressed & PadState::A) && focus_ >= 0) {
        Slot& s = slots_[(size_t)focus_];
        /* NEpochSlot.ClickedSfx (a slot that opens something) */
        if ((s.state == "Obtained" || s.state == "Complete") && lib_->audio())
            lib_->audio()->ui("event:/sfx/ui/timeline/ui_timeline_click");
        if (s.state == "Obtained") {
            /* NEpochSlot.RevealEpoch. */
            *cmd = "{\"cmd\":\"reveal_epoch\",\"epoch\":\"" + s.id + "\"}";
            mode_ = WAITING;
            clear_tip();
        } else if (s.state == "Complete") {
            open_inspect(s.data, false);
        }
    }
    return NONE;
}

void TimelineScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    unlock_t_ += dt;
    if (!held_.is_null() && unlock_t_ >= tutorial_close_at_) {
        /* the tutorial has faded out: SpawnFirstTimeTimeline */
        Json d = held_;
        held_ = Json();
        tutorial_close_at_ = -1;
        set_data(d);
    }
    /* NEpochOffscreenVfx._Process: an obtained slot off the screen shows
     * at the edge it went past (0.75 in 0.5 s, 60 px under its top); back
     * on the screen it goes (0.2 s). */
    for (size_t i = 0; i < slots_.size(); ++i) {
        Slot& s = slots_[i];
        if (!s.offscreen || !s.node) continue;
        Vec2 at = s.node->global_transform().origin;
        if (!s.off_shown && (at.x < 0 || at.x > 1920)) {
            s.off_shown = true;
            tweens_.tween(&s.offscreen->modulate.a, 0.75f, 0.5f);
            Vec2 edge(at.x < 0 ? 0.0f : 1920.0f, at.y + 60);
            Node* p = s.offscreen->parent;
            s.offscreen->set_position(p && p->is_canvas_item() ? inverse(((CanvasItem*)p)->global_transform()).xform(edge) : edge);
        } else if (s.off_shown && at.x > 0 && at.x < 1920) {
            s.off_shown = false;
            tweens_.tween(&s.offscreen->modulate.a, 0.0f, 0.2f);
        }
    }
    /* NEpochSlot._Process: the obtained slots' outline pulses. */
    float a = (float)(sin(t_ * 1000.0 * 0.005) + 2.0) * 0.25f;
    for (size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].pulsing && slots_[i].outline) slots_[i].outline->modulate.a = a;
    /* The pan towards the focused slot. */
    if (moved_) {
        float step = (pan_target_ - pan_) * (float)(dt * 8.0 > 1.0 ? 1.0 : dt * 8.0);
        pan_ += step;
        float w = moved_->offset[SIDE_RIGHT] - moved_->offset[SIDE_LEFT];
        moved_->offset[SIDE_LEFT] += step;
        moved_->offset[SIDE_RIGHT] = moved_->offset[SIDE_LEFT] + w;
    }
    if (info_text_at_ >= 0 && unlock_t_ >= info_text_at_) {
        info_text_at_ = -1;
        Control* info = inspect_ ? ctl(inspect_, "UnlockInfo") : NULL;
        if (info) {
            set_label(ctl(info, "Text"), info_text_);
            tweens_.tween(&info->modulate.a, 0.8f, 1.0f);
        }
    }
    if (mode_ == UNLOCKS && closing_unlock_ && unlock_t_ >= 1.0) {
        if (unlock_->parent) unlock_->parent->remove_child(unlock_);
        delete unlock_;
        unlock_ = NULL;
        closing_unlock_ = false;
        next_unlock();
    } else if (mode_ == UNLOCKS && !unlock_ && unlock_t_ >= 3.5) {
        /* The timeline expansion has played. */
        next_unlock();
    }
}

} /* namespace spire */
