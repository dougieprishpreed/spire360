#include "compendium_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "card_library_screen.h"
#include "loc.h"

#include <string.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

CompendiumScreen::CompendiumScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), focus_(0), library_(NULL), sub_node_(NULL) {}

CompendiumScreen::~CompendiumScreen() { close_sub(); }

bool CompendiumScreen::is_answer(const std::string& type) { return type == "library"; }

Node* CompendiumScreen::build(const std::string& character, std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/compendium_submenu.tscn");
    if (!screen) {
        if (error) *error = "no compendium_submenu.tscn";
        return NULL;
    }
    character_ = character;
    Control* root = full_rect_control("Compendium");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");
    /* SetIconAndLocalization / SetLocalization */
    static const struct {
        const char* node;
        const char* loc;
        const char* key;
        bool shown;
    } all[] = { { "CardLibraryButton", "COMPENDIUM_CARD_LIBRARY", "library", true },
                { "RelicCollectionButton", "COMPENDIUM_RELIC_COLLECTION", "relics", false },
                { "PotionLabButton", "COMPENDIUM_POTION_LAB", "potions", false },
                { "BestiaryButton", "COMPENDIUM_BESTIARY", "bestiary", false },
                { "LeaderboardsButton", "LEADERBOARDS", "", false },
                { "AchievementsButton", "ACHIEVEMENTS", "", false },
                { "StatisticsButton", "STATISTICS", "stats", false },
                { "RunHistoryButton", "RUN_HISTORY", "history", false } };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        Control* b = control_of(screen->find_unique(all[i].node));
        if (!b) continue;
        Node* title = find_named(b, "Title");
        if (!title) title = find_named(b, "Label");
        set_label(title, Loc::get("main_menu_ui", (std::string(all[i].loc) + ".title").c_str()));
        set_label(find_named(b, "Description"), Loc::get("main_menu_ui", (std::string(all[i].loc) + ".description").c_str()));
        if (i < 4)
            set_texture(lib_, find_named(b, "Icon"),
                        std::string("images/packed/main_menu/submenu_icon_") + lower(all[i].loc) + ".png");
        hide(b->get_node("Lock"));
        if (!all[i].shown) {
            hide(b);
            continue;
        }
        Button bt;
        bt.node = b;
        bt.key = all[i].key;
        buttons_.push_back(bt);
    }
    focus(0);
    root->add_child(frame_driver(this));
    return root;
}

void CompendiumScreen::focus(int i) {
    if (buttons_.empty()) return;
    if (i < 0) i = 0;
    if (i >= (int)buttons_.size()) i = (int)buttons_.size() - 1;
    focus_ = i;
    for (size_t k = 0; k < buttons_.size(); ++k) {
        /* NShortSubmenuButton.OnFocus: 1.02 */
        Control* b = buttons_[k].node;
        tweens_.tween(&b->scale, (int)k == i ? Vec2(1.02f, 1.02f) : Vec2(1, 1), 0.05f);
        set_visible(find_named(b, "SelectionReticle"), (int)k == i);
    }
}

void CompendiumScreen::close_sub() {
    if (sub_node_) {
        if (sub_node_->parent) sub_node_->parent->remove_child(sub_node_);
        delete sub_node_;
        sub_node_ = NULL;
    }
    delete library_;
    library_ = NULL;
}

void CompendiumScreen::set_answer(const Json& msg) {
    if (library_ && msg["type"].str() == "library") library_->set_cards(msg);
}

CompendiumScreen::Action CompendiumScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (library_) {
        if (library_->input(pad, cmd) == CardLibraryScreen::BACK) close_sub();
        return NONE;
    }
    if (pad.pressed & PadState::B) return BACK;
    if (pad.pressed & (PadState::LEFT | PadState::UP)) focus(focus_ - 1);
    if (pad.pressed & (PadState::RIGHT | PadState::DOWN)) focus(focus_ + 1);
    if ((pad.pressed & PadState::A) && focus_ >= 0 && focus_ < (int)buttons_.size()) {
        const char* key = buttons_[(size_t)focus_].key;
        std::string err;
        if (strcmp(key, "library") == 0) {
            library_ = new CardLibraryScreen(lib_);
            sub_node_ = library_->build(character_, &err);
            if (sub_node_) {
                root_->add_child(sub_node_);
                *cmd = library_->first_query();
            } else {
                delete library_;
                library_ = NULL;
            }
        }
    }
    return NONE;
}

void CompendiumScreen::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */
