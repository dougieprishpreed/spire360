#include "bestiary_screen.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/value.h"
#include "../spine/spine.h"
#include "combat_screen.h"
#include "game.h"
#include "loc.h"
#include "screen_shake.h"

#include <math.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }
static CanvasItem* item(Node* n) { return n && n->is_canvas_item() ? (CanvasItem*)n : NULL; }

/* StsColors */
static const unsigned CREAM = 0xFFF6E2, RED = 0xFF5555, PURPLE = 0xEE82EE, BLUE = 0x87CEEB, GREEN = 0x7FFF00;
static const Color GRAY(0.5f, 0.5f, 0.5f, 1.0f);

/* NBestiary's move hotkeys with a controller: the button glyph and pad bit */
static const struct {
    const char* glyph;
    unsigned bit;
} MOVE_KEYS[9] = { { "lb", PadState::LB },          { "rb", PadState::RB },           { "rt", PadState::RT },
                   { "lt", PadState::LT },          { "x", PadState::X },             { "rs_up", PadState::RS_UP },
                   { "rs_down", PadState::RS_DOWN }, { "rs_left", PadState::RS_LEFT }, { "rs_right", PadState::RS_RIGHT } };

static Vec2 global_origin(Node* n) {
    return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2(0, 0);
}

/* An id as a JSON string (ids are plain names). */
static std::string quoted(const std::string& s) { return "\"" + s + "\""; }

/* "#rrggbbaa" (the logic's colours). */
static Color rgba(const std::string& s) {
    unsigned v = 0;
    for (size_t i = 1; i < s.size() && i < 9; ++i) {
        char c = s[i];
        v = v * 16 + (unsigned)(c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
    }
    return Color((float)((v >> 24) & 255) / 255.0f, (float)((v >> 16) & 255) / 255.0f, (float)((v >> 8) & 255) / 255.0f,
                 (float)(v & 255) / 255.0f);
}

static SpineSprite* first_spine(Node* n) {
    if (!n) return NULL;
    if (n->type == "SpineSprite") return (SpineSprite*)n;
    for (size_t i = 0; i < n->children.size(); ++i) {
        SpineSprite* s = first_spine(n->children[i]);
        if (s) return s;
    }
    return NULL;
}

/* The animation a creature's CreatureAnimator trigger plays. */
static std::string trigger_animation(const std::string& trigger) {
    if (trigger == "Attack") return "attack";
    if (trigger == "Cast") return "cast";
    if (trigger == "Hit") return "hurt";
    if (trigger == "Dead") return "die";
    if (trigger == "Idle") return "idle_loop";
    return lower(trigger);
}

/* Position y / x of a control (Godot's position:y / position:x) from one
 * value to its own, moving both offsets. */
static void tween_offset_y(TweenSet& tw, Control* c, float from, float to, float seconds, float delay = 0) {
    if (!c) return;
    float h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
    tw.tween_from(&c->offset[SIDE_TOP], from, to, seconds, TRANS_EXPO, EASE_OUT, delay);
    tw.tween_from(&c->offset[SIDE_BOTTOM], from + h, to + h, seconds, TRANS_EXPO, EASE_OUT, delay);
}

static void tween_offset_x(TweenSet& tw, Control* c, float from, float to, float seconds, float delay = 0) {
    if (!c) return;
    float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT];
    tw.tween_from(&c->offset[SIDE_LEFT], from, to, seconds, TRANS_EXPO, EASE_OUT, delay);
    tw.tween_from(&c->offset[SIDE_RIGHT], from + w, to + w, seconds, TRANS_EXPO, EASE_OUT, delay);
}

BestiaryScreen::BestiaryScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), screen_(NULL), layout_(NULL), arrow_(NULL), focus_(-1), selected_(-1), filter_(0),
      arrow_target_(-1), stats_mode_(false), arrow_placed_(false), t_(0), cue_t_(0), next_cue_(0) {}

Node* BestiaryScreen::build(std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/bestiary/bestiary.tscn");
    if (!screen) {
        if (error) *error = "no bestiary.tscn";
        return NULL;
    }
    screen_ = screen;
    Control* root = full_rect_control("Bestiary");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    show_glyph(lib_, find_named(screen->find_unique("BackButton"), "HotkeyIcon"), "b");
    /* _Ready */
    set_label(screen->find_unique("MoveHeader"), Loc::get("bestiary", "ACTIONS.header"));
    set_label(screen->find_unique("ConstructionLabel"), Loc::get("bestiary", "UNDER_CONSTRUCTION"));
    show_glyph(lib_, screen->find_unique("ModeButton") ? screen->find_unique("ModeButton")->find_unique("HotkeyIcon") : NULL, "y");
    show_glyph(lib_, screen->find_unique("PageLeftIcon"), "lb");
    show_glyph(lib_, screen->find_unique("PageRightIcon"), "rb");
    hide(screen->find_unique("MonsterName"));
    /* The selection arrow moves freely beside the entries: a plain holder
     * at the list's origin keeps the content's layout off it. */
    Node* content = screen->find_unique("Sidebar") ? screen->find_unique("Sidebar")->get_node("Content") : NULL;
    Node* arrow = screen->find_unique("SelectionArrow");
    if (content && arrow && arrow->parent) {
        arrow->parent->remove_child(arrow);
        Control* holder = (Control*)SceneLibrary::create_node("Control");
        holder->name = "ArrowHolder";
        content->add_child(holder);
        holder->add_child(arrow);
        arrow_ = control_of(arrow);
        if (arrow_) arrow_->anchor[SIDE_LEFT] = arrow_->anchor[SIDE_TOP] = arrow_->anchor[SIDE_RIGHT] = arrow_->anchor[SIDE_BOTTOM] = 0;
    }
    scroll_.attach(screen->find_unique("Sidebar"));
    if (screen->is_canvas_item()) tweens_.tween_from(&((CanvasItem*)screen)->modulate.a, 0.0f, 1.0f, 0.4f);
    root->add_child(frame_driver(this));
    return root;
}

void BestiaryScreen::set_data(const Json& msg) {
    const std::string& type = msg["type"].str();
    if (type == "bestiary") {
        if (!entries_.empty()) return;
        data_ = msg;
        fill_list();
        set_mode(data_["stats_mode"].b(), false);
        /* CreateEntries: the first discovered one selected */
        if (!entries_.empty()) {
            focus(0);
            select(0);
        }
    } else if (type == "bestiary_entry") {
        if (selected_ >= 0 && msg["encounter"].str() == (*entries_[(size_t)selected_].data)["encounter"].str() &&
            msg["monster"] == (*entries_[(size_t)selected_].data)["monster"])
            show_entry(msg);
    } else if (type == "bestiary_move") {
        if (msg["ok"].b()) {
            std::vector<std::string> who;
            for (size_t i = 0; i < msg["who"].size(); ++i) who.push_back(msg["who"][i].str());
            for (size_t i = 0; i < shown_.size() && i < who.size(); ++i) shown_[i].who = who[i];
            play_cues(msg["steps"], who.empty() ? std::string() : who[0]);
        }
    }
}

/* AddAct / AddEvents: a label divider per section, then its entries
 * (NBestiaryEntry: the room type's colour; unseen ones grey and disabled). */
void BestiaryScreen::fill_list() {
    Node* list = screen_->find_unique("BestiaryList");
    if (!list) return;
    const Json& sections = data_["sections"];
    for (size_t s = 0; s < sections.size(); ++s) {
        Node* div = instance(lib_, "scenes/screens/bestiary/bestiary_label_divider.tscn");
        if (div) {
            Node* label = div->get_node("Label");
            set_label(label, sections[s]["title"].str());
            if (CanvasItem* l = item(label)) l->modulate = hex(BLUE);
            list->add_child(div);
        }
        const Json& es = sections[s]["entries"];
        for (size_t i = 0; i < es.size(); ++i) {
            Control* n = control_of(instance(lib_, "scenes/screens/bestiary/bestiary_entry.tscn"));
            if (!n) continue;
            Control* label = control_of(n->find_unique("Label"));
            set_label(label, es[i]["title"].str());
            hide(n->find_unique("Highlight"));
            hide(n->find_unique("UnderConstructionIcon"));
            if (label) {
                const std::string& room = es[i]["room_type"].str();
                label->self_modulate = !es[i]["discovered"].b() ? GRAY
                                                               : hex(room == "Boss" ? RED : room == "Elite" ? PURPLE : CREAM);
            }
            list->add_child(n);
            if (!es[i]["discovered"].b()) continue;
            Entry e = { n, label, &es[i] };
            entries_.push_back(e);
        }
    }
    /* the character filters (CreateFilters) */
    Node* grid = screen_->find_unique("PoolFilters");
    const Json& fs = data_["filters"];
    for (size_t i = 0; grid && i < fs.size(); ++i) {
        Control* n = control_of(instance(lib_, "scenes/screens/bestiary/bestiary_character_filter.tscn"));
        if (!n) continue;
        Control* image = control_of(n->find_unique("Image"));
        if (fs[i]["character"].is_string()) {
            std::string icon = "images/ui/top_panel/character_icon_" + fs[i]["character"].str() + ".png";
            set_texture(lib_, image, icon);
            set_texture(lib_, n->find_unique("Shadow"), icon);
        }
        hide(n->find_unique("SelectionReticle"));
        grid->add_child(n);
        Filter f = { n, image, false };
        filters_.push_back(f);
    }
    select_filter(0);
}

void BestiaryScreen::focus(int i) {
    if (i < 0 || i >= (int)entries_.size()) return;
    if (focus_ >= 0 && focus_ < (int)entries_.size()) {
        /* OnUnfocus */
        Entry& o = entries_[(size_t)focus_];
        if (o.label) tweens_.tween(&o.label->scale, Vec2(1, 1), 0.3f, TRANS_CUBIC, EASE_OUT);
        hide(o.node->find_unique("Highlight"));
    }
    focus_ = i;
    Entry& e = entries_[(size_t)i];
    /* OnFocus: the label grows, the highlight shows (directional navigation) */
    if (e.label) {
        e.label->pivot_offset = offset_size(e.label) * 0.5f;
        tweens_.tween(&e.label->scale, Vec2(1.1f, 1.1f), 0.05f);
    }
    set_visible(e.node->find_unique("Highlight"), true);
    scroll_.focus(e.node);
}

/* SelectMonster: the name, the panel, the layout (the logic's answer fills
 * it), the arrow beside the entry. */
void BestiaryScreen::select(int i) {
    if (i < 0 || i >= (int)entries_.size() || i == selected_) return;
    selected_ = i;
    const Json& e = *entries_[(size_t)i].data;
    Control* name = control_of(screen_->find_unique("MonsterName"));
    set_visible(name, true);
    set_label(name, e["title"].str());
    if (name) {
        name->self_modulate.a = 0;
        tweens_.tween(&name->self_modulate.a, 1.0f, 0.5f);
        tween_offset_y(tweens_, name, 24, 88, 0.5f);
    }
    Control* dialogue = control_of(screen_->find_unique("DialogueText"));
    if (dialogue) tweens_.tween_from(&dialogue->modulate.a, 0.0f, 1.0f, 0.5f);
    set_mode(stats_mode_, true);
    clear_layout();
    cues_.clear();
    next_cue_ = 0;
    /* The layout and its creature now; the moves come with the logic's
     * answer (which reads the creature's skeleton's animations). */
    layout_kind_ = e["encounter"].str() == "KAISER_CRAB_BOSS"      ? "kaiser_crab"
                   : e["encounter"].str() == "DECIMILLIPEDE_ELITE" ? "decimillipede"
                                                                    : "default";
    layout_ = control_of(instance(lib_, "scenes/screens/bestiary/bestiary_layout_" + layout_kind_ + ".tscn"));
    Node* container = screen_->find_unique("LayoutContainer");
    if (layout_ && container) container->add_child(layout_);
    std::string anims;
    if (layout_ && layout_kind_ != "decimillipede" && e["monster"].is_string()) {
        Json view;
        view.set("id", Json::make_string(lower(e["monster"].str())));
        Shown s;
        s.node = make_display_creature(lib_, view, &s.visuals);
        s.view = view;
        if (s.node) {
            Node* parent = layout_kind_ == "default" ? layout_->find_unique("MonsterVisualsContainer") : (Node*)layout_;
            if (parent) parent->add_child(s.node);
            Rect2 h = creature_hitbox(s.visuals);
            s.node->set_position(Vec2(0, h.size.y * 0.5f));
            s.node->modulate = Color(0, 0, 0, 0);
            tweens_.tween(&s.node->modulate.r, 1.0f, 0.25f);
            tweens_.tween(&s.node->modulate.g, 1.0f, 0.25f);
            tweens_.tween(&s.node->modulate.b, 1.0f, 0.25f);
            tweens_.tween(&s.node->modulate.a, 1.0f, 0.25f);
            shown_.push_back(s);
            SpineSprite* sp = first_spine(s.visuals);
            static const char* const checked[3] = { "revive", "hurt", "die" };
            for (int k = 0; k < 3; ++k)
                if (sp && sp->has_animation(checked[k])) anims += std::string(anims.empty() ? "" : ",") + "\"" + checked[k] + "\"";
        }
    }
    if (layout_kind_ == "kaiser_crab" && layout_) {
        /* the background moved so this part's arm is in view */
        Node* crab = layout_->find_unique("KaiserCrab");
        if (crab && crab->is_node2d()) {
            Node2D* c = (Node2D*)crab;
            float sx = c->scale.x != 0 ? c->scale.x : 1;
            if (e["monster"].str() == "CRUSHER") c->position.x += 1020.0f * 0.5f / sx;
            else if (e["monster"].str() == "ROCKET") c->position.x -= 1240.0f * 0.5f / sx;
        }
    }
    anims_ = "[" + anims + "]";
    pending_ = "{\"cmd\":\"bestiary_entry\",\"encounter\":" + quoted(e["encounter"].str()) +
               (e["monster"].is_string() ? ",\"monster\":" + quoted(e["monster"].str()) : std::string()) + ",\"anims\":" + anims_ +
               "}";
    /* the arrow: shown at its place a frame after the first selection, then
     * sliding to each next one */
    if (arrow_) {
        if (!arrow_placed_) arrow_->modulate.a = 0;
        arrow_target_ = i;
    }
    refresh_stats();
}

void BestiaryScreen::clear_layout() {
    for (size_t i = 0; i < moves_.size(); ++i) moves_[i]->queue_free();
    moves_.clear();
    move_data_ = Json();
    shown_.clear();
    if (layout_) layout_->queue_free();
    layout_ = NULL;
}

/* The logic's NBestiaryLayout.Setup: the monsters' spine setup (made here
 * for the Decimillipede, in its encounter slots), the move buttons. */
void BestiaryScreen::show_entry(const Json& msg) {
    entry_ = msg;
    const Json& ms = msg["monsters"];
    if (layout_kind_ == "decimillipede" && layout_ && shown_.empty()) {
        Node* container = layout_->find_unique("CreatureContainer");
        for (size_t i = 0; container && i < ms.size(); ++i) {
            Json view = ms[i];
            view.set("id", Json::make_string(lower(ms[i]["id"].str())));
            Shown s;
            s.node = make_display_creature(lib_, view, &s.visuals);
            s.view = view;
            if (!s.node) continue;
            container->add_child(s.node);
            s.slot = ms[i]["slot"].str(); /* placed each frame (frame()), once the layout has its rects */
            shown_.push_back(s);
        }
    } else {
        for (size_t i = 0; i < shown_.size() && i < ms.size(); ++i) setup_creature_spine(shown_[i].visuals, ms[i]);
    }
    move_data_ = msg["moves"];
    make_moves();
}

void BestiaryScreen::make_moves() {
    Node* list = screen_->find_unique("MoveList");
    for (size_t i = 0; list && i < move_data_.size() && i < 9; ++i) {
        Control* b = control_of(instance(lib_, "scenes/screens/bestiary/bestiary_move_button.tscn"));
        if (!b) continue;
        set_label(b->find_unique("Label"), move_data_[i]["name"].str());
        show_glyph(lib_, b->find_unique("HotkeyIcon"), MOVE_KEYS[i].glyph);
        list->add_child(b);
        moves_.push_back(b);
    }
}

/* OnMoveButtonClicked / PlayMoveAnim: an animation move plays here (its
 * sound with it, then the idle unless it is the death); the others are the
 * logic's to play. */
void BestiaryScreen::play_move(int i, std::string* cmd) {
    if (i < 0 || i >= (int)moves_.size() || selected_ < 0) return;
    const Json& m = move_data_[(size_t)i];
    /* NBestiaryMoveButton.OnRelease: the label green back to cream, the button bouncing back */
    Control* label = control_of(moves_[(size_t)i]->find_unique("Label"));
    if (label) {
        label->modulate = hex(GREEN);
        Color cream = hex(CREAM);
        tweens_.tween(&label->modulate.r, cream.r, 0.2f);
        tweens_.tween(&label->modulate.g, cream.g, 0.2f);
        tweens_.tween(&label->modulate.b, cream.b, 0.2f);
    }
    Control* anim = control_of(moves_[(size_t)i]->find_unique("ButtonAnimator"));
    if (anim) tweens_.tween_from(&anim->scale, Vec2(0.9f, 0.9f), Vec2(1, 1), 0.3f, TRANS_BOUNCE, EASE_OUT);
    if (!m["logic"].b()) {
        for (size_t k = 0; k < shown_.size(); ++k) {
            SpineSprite* s = first_spine(shown_[k].visuals);
            if (!s) continue;
            s->set_animation(m["anim"].str(), false, 0);
            if (m["anim"].str() != "die") s->add_animation("idle_loop", true, 0, 0);
        }
        if (m["sfx"].is_string() && lib_->audio()) {
            Json cue;
            cue.set("t", Json::make_string("sfx"));
            cue.set("op", Json::make_string("play"));
            cue.set("path", m["sfx"]);
            cue.set("volume", Json::make_number(1));
            lib_->audio()->cue(cue);
        }
        return;
    }
    const Json& e = *entries_[(size_t)selected_].data;
    *cmd = format("{\"cmd\":\"bestiary_move\",\"index\":%d,\"encounter\":", i) + quoted(e["encounter"].str()) +
           (e["monster"].is_string() ? ",\"monster\":" + quoted(e["monster"].str()) : std::string()) + ",\"anims\":" + anims_ + "}";
}

/* The move's steps as the fight plays them: in order, waiting where the game
 * waits (the animation triggers' waits, Cmd.Wait, the crab's arm). */
void BestiaryScreen::play_cues(const Json& steps, const std::string& who) {
    (void)who;
    cues_.clear();
    next_cue_ = 0;
    cue_t_ = 0;
    double t = 0;
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& ch = steps[i][k];
            const std::string& type = ch["t"].str();
            if (type == "wait") {
                t += ch["ms"].num(0) / 1000.0;
                continue;
            }
            Cue c = { t, ch };
            cues_.push_back(c);
            if (type == "anim") t += ch["wait"].num(0);
            if (type == "ui") t += kaiser_crab_wait(ch["call"].str(), ch["args"]);
        }
}

void BestiaryScreen::fire(const Json& ch) {
    const std::string& type = ch["t"].str();
    const std::string& call = ch["call"].str();
    if (type == "sfx" || (type == "ui" && (call.find("Audio") != std::string::npos || call.find("Music") != std::string::npos))) {
        if (lib_->audio()) lib_->audio()->cue(ch);
        return;
    }
    if (type == "anim") {
        Shown* s = shown_for(ch["who"].str());
        SpineSprite* sp = s ? first_spine(s->visuals) : NULL;
        if (ch["play"].is_array()) {
            play_animator_ops(sp, ch["play"]);
            return;
        }
        std::string anim = trigger_animation(ch["name"].str());
        if (!sp || !sp->set_animation(anim, false, 0)) return;
        if (anim != "die") sp->add_animation("idle_loop", true, 0, 0);
        return;
    }
    Node* box = screen_->find_unique("VfxContainer");
    if (type == "vfx") {
        Vec2 at(0, 0);
        VfxCreature vc;
        if (ch["at"].str() == "screen") at = Vec2(960, 540);
        else if (ch["side"].is_string()) side_center(ch["side"].str() != "player", &at);
        else if (creature(ch["who"].str(), &vc)) at = ch["at"].str() == "feet" ? vc.bottom : vc.center;
        Node* fx = instance(lib_, "scenes/" + ch["path"].str() + ".tscn");
        if (!fx || !box) {
            delete fx;
            return;
        }
        if (fx->is_node2d()) ((Node2D*)fx)->position = at - global_origin(box);
        else if (fx->is_control()) ((Control*)fx)->set_position(at - global_origin(box));
        box->add_child(fx);
        return;
    }
    if (type != "ui") return;
    double wait;
    if (kaiser_crab_call(call, ch["args"], &wait)) return;
    if (ScreenShake::get().cue(call, ch["args"])) return;
    if (is_combat_vfx_call(call) && box) spawn_combat_vfx(lib_, box, call, ch["args"], *this);
}

BestiaryScreen::Shown* BestiaryScreen::shown_for(const std::string& who) {
    for (size_t i = 0; i < shown_.size(); ++i)
        if (shown_[i].who == who) return &shown_[i];
    return NULL;
}

bool BestiaryScreen::creature(const std::string& who, VfxCreature* out) {
    Shown* s = shown_for(who);
    if (!s || !s->node) return false;
    Xform2D xf = s->node->global_transform();
    Rect2 h = creature_hitbox(s->visuals);
    Node* center = s->visuals ? s->visuals->find_unique("CenterPos") : NULL;
    out->center = center ? global_origin(center) : xf.xform(h.pos + h.size * 0.5f);
    out->top = xf.xform(h.pos + Vec2(h.size.x * 0.5f, 0));
    out->bottom = xf.xform(h.pos + Vec2(h.size.x * 0.5f, h.size.y));
    out->hitbox = Rect2(xf.xform(h.pos), h.size);
    out->enemy = true;
    out->interactable = true;
    out->defect = false;
    return true;
}

/* VfxCmd.GetSideCenter outside a fight: the bestiary's creatures (NBestiary.
 * GetSideCenter) for the enemies, the origin for the player's side. */
bool BestiaryScreen::side_center(bool enemies, Vec2* out) {
    *out = Vec2(0, 0);
    if (!enemies) return true;
    int n = 0;
    Vec2 sum(0, 0);
    for (size_t i = 0; i < shown_.size(); ++i) {
        VfxCreature vc;
        if (creature(shown_[i].who, &vc)) {
            sum = sum + vc.center;
            ++n;
        }
    }
    if (n) *out = sum * (1.0f / (float)n);
    return true;
}

Node* BestiaryScreen::back_box() { return screen_ ? screen_->find_unique("BackVfxContainer") : NULL; }

Node* BestiaryScreen::take_visuals(const std::string&) { return NULL; }

/* ToggleMode / ShowMovesPanel / ShowStatsPanel: the panel slides in from
 * x 210 to 242 and fades in over 0.5 s. */
void BestiaryScreen::set_mode(bool stats, bool animate) {
    stats_mode_ = stats;
    Control* moves = control_of(screen_->find_unique("MoveContainer"));
    Control* panel = control_of(screen_->find_unique("StatsContainer"));
    set_visible(moves, !stats);
    set_visible(panel, stats);
    Control* shown = stats ? panel : moves;
    if (shown && animate) {
        shown->modulate.a = 0;
        tween_offset_x(tweens_, shown, 210, 242, 0.5f);
        tweens_.tween(&shown->modulate.a, 1.0f, 0.5f);
    }
    Node* mode = screen_->find_unique("ModeButton");
    set_label(mode ? mode->find_unique("ModeLabel") : NULL,
              Loc::get("bestiary", stats ? "MODE.viewActions" : "MODE.viewStats"));
    set_visible(screen_->find_unique("PageLeftIcon"), stats);
    set_visible(screen_->find_unique("PageRightIcon"), stats);
    refresh_stats();
}

/* NBestiaryCharacterFilter.IsSelected: its image lit and grown (s / v 1.1,
 * scale 1.2) or dimmed (0.5 / 0.75, 0.95). */
void BestiaryScreen::select_filter(int i) {
    if (i < 0 || i >= (int)filters_.size()) return;
    filter_ = i;
    for (size_t k = 0; k < filters_.size(); ++k) {
        Control* img = filters_[k].image;
        if (!img) continue;
        bool on = (int)k == i;
        img->shader_params["s"] = on ? 1.1f : 0.5f;
        img->shader_params["v"] = on ? 1.1f : 0.75f;
        img->pivot_offset = offset_size(img) * 0.5f;
        tweens_.tween(&img->scale, on ? Vec2(1.2f, 1.2f) : Vec2(0.95f, 0.95f), on ? 0.2f : 0.3f, on ? TRANS_BACK : TRANS_EXPO,
                      EASE_OUT);
    }
    refresh_stats();
}

/* FilterLeft / FilterRight: the next unlocked one that way. */
void BestiaryScreen::step_filter(int dir) {
    int n = (int)filters_.size();
    for (int k = 1; k <= n; ++k) {
        int j = ((filter_ + dir * k) % n + n) % n;
        if (!filters_[(size_t)j].locked) {
            select_filter(j);
            return;
        }
    }
}

/* RefreshStatisticsText + DisplayCharacterData: each filter's lock (no fight
 * yet), the selected one's numbers and, for a character, its quote in its
 * colours (shown in the stats mode only). */
void BestiaryScreen::refresh_stats() {
    if (selected_ < 0 || !screen_) return;
    const Json& stats = (*entries_[(size_t)selected_].data)["stats"];
    for (size_t k = 0; k < filters_.size(); ++k) {
        const Json& s = stats[k];
        filters_[k].locked = s["kills"].i() + s["deaths"].i() <= 0;
        if (filters_[k].image) filters_[k].image->self_modulate = filters_[k].locked ? Color(1, 1, 1, 0.2f) : Color(1, 1, 1, 1);
    }
    if (filter_ > 0 && filter_ < (int)filters_.size() && filters_[(size_t)filter_].locked) {
        select_filter(0);
        return;
    }
    if (!stats_mode_) {
        Control* line = control_of(screen_->find_unique("DialogueLine"));
        if (line) tweens_.tween(&line->modulate.a, 0.0f, 0.1f);
        return;
    }
    const Json& s = stats[(size_t)filter_];
    set_label(screen_->find_unique("StatisticsFull"), s["text"].str());
    const Json& f = data_["filters"][(size_t)filter_];
    Control* line = control_of(screen_->find_unique("DialogueLine"));
    if (!f["character"].is_string()) {
        if (line) tweens_.tween(&line->modulate.a, 0.0f, 0.1f);
        return;
    }
    std::string quote = s["kills"].i() <= 0 ? f["seen_quote"].str()
                        : f["kill_quote"].is_string() ? f["kill_quote"].str()
                                                      : data_["placeholder_quote"].str();
    set_label(screen_->find_unique("DialogueText"), quote);
    /* UpdateDialogueBubbleStyle */
    std::string c = f["character"].str();
    set_texture(lib_, screen_->find_unique("Icon"), "images/ui/top_panel/character_icon_" + c + ".png");
    set_texture(lib_, screen_->find_unique("Outline"), "images/ui/top_panel/character_icon_" + c + "_outline.png");
    Color col = rgba(f["dialogue_color"].str());
    if (CanvasItem* b = item(screen_->find_unique("Bubble"))) b->self_modulate = col;
    Node* tail = screen_->find_unique("DialogueTail");
    if (CanvasItem* t = item(tail)) t->self_modulate = col;
    set_texture(lib_, tail, f["thought"].b() ? "images/ui/thought_tail.png" : "images/ui/dialogue_tail.png");
    /* ShowDialogue */
    if (line) {
        line->modulate.a = 0;
        tweens_.tween(&line->modulate.a, 1.0f, 0.1f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
        tween_offset_x(tweens_, line, 528 - 960, 560 - 960, 0.4f, 0.1f);
    }
}

BestiaryScreen::Action BestiaryScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (!pending_.empty()) {
        *cmd = pending_;
        pending_.clear();
        return NONE;
    }
    if (pad.pressed & PadState::B) {
        /* OnSubmenuClosed: the mode it was left in */
        *cmd = std::string("{\"cmd\":\"set_setting\",\"name\":\"bestiary_actions\",\"value\":") + (stats_mode_ ? "false" : "true") + "}";
        return BACK;
    }
    if (pad.pressed & PadState::UP) focus(focus_ - 1);
    if (pad.pressed & PadState::DOWN) focus(focus_ + 1);
    if (pad.pressed & PadState::A) select(focus_);
    if (pad.pressed & PadState::Y) set_mode(!stats_mode_, true);
    if (stats_mode_) {
        if (pad.pressed & PadState::LB) step_filter(-1);
        if (pad.pressed & PadState::RB) step_filter(1);
        return NONE;
    }
    for (int i = 0; i < 9; ++i)
        if (pad.pressed & MOVE_KEYS[i].bit) {
            play_move(i, cmd);
            break;
        }
    return NONE;
}

void BestiaryScreen::frame(double dt) {
    tweens_.update(dt);
    scroll_.frame(dt);
    t_ += dt;
    cue_t_ += dt;
    while (next_cue_ < cues_.size() && cues_[next_cue_].at <= cue_t_) fire(cues_[next_cue_++].data);
    /* the Decimillipede's segments at their slots (GlobalPosition = the
     * EncounterSlots marker's) */
    Node* slots = layout_ ? layout_->find_unique("EncounterSlots") : NULL;
    for (size_t i = 0; slots && i < shown_.size(); ++i) {
        Node* marker = shown_[i].slot.empty() ? NULL : find_named(slots, shown_[i].slot.c_str());
        if (marker && shown_[i].node->parent)
            shown_[i].node->set_position(global_origin(marker) - global_origin(shown_[i].node->parent));
    }
    /* the arrow beside the selected entry (its place in the list + (-42, 119),
     * the list starting 120 down in the content) */
    if (arrow_ && arrow_target_ >= 0 && arrow_target_ < (int)entries_.size()) {
        Vec2 to = offset_pos(entries_[(size_t)arrow_target_].node) + Vec2(-42, -1);
        if (!arrow_placed_) {
            arrow_placed_ = true;
            arrow_->set_position(to);
            tweens_.tween(&arrow_->modulate.a, 1.0f, 0.2f);
        } else {
            arrow_->modulate.a = 1;
            Vec2 now = offset_pos(arrow_);
            tweens_.tween_from(&arrow_->offset[SIDE_LEFT], now.x, to.x, 0.25f, TRANS_EXPO, EASE_OUT);
            tweens_.tween_from(&arrow_->offset[SIDE_TOP], now.y, to.y, 0.25f, TRANS_EXPO, EASE_OUT);
            tweens_.tween_from(&arrow_->offset[SIDE_RIGHT], now.x + offset_size(arrow_).x, to.x + offset_size(arrow_).x, 0.25f,
                               TRANS_EXPO, EASE_OUT);
            tweens_.tween_from(&arrow_->offset[SIDE_BOTTOM], now.y + offset_size(arrow_).y, to.y + offset_size(arrow_).y, 0.25f,
                               TRANS_EXPO, EASE_OUT);
        }
        arrow_target_ = -1;
    }
}

} /* namespace spire */
