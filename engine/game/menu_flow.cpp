#include "menu_flow.h"
#include "../audio/game_audio.h"

#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/tree.h"
#include "character_select.h"
#include "confirm_popup.h"
#include "ftue.h"
#include "game_client.h"
#include "profile_screen.h"
#include "timeline_screen.h"
#include "settings_screen.h"
#include "compendium_screen.h"
#include "title_screen.h"
#include "logo_screen.h"
#include "ui_util.h"
#include "loc.h"

namespace spire {

MenuFlow::MenuFlow(SceneLibrary* lib, SceneTree* tree, GameClient* game, const std::string& version)
    : lib_(lib), tree_(tree), game_(game), version_(version), logo_(NULL), logo_node_(NULL), title_(new TitleScreen(lib)), title_node_(NULL),
      chars_(new CharacterSelectScreen(lib)), confirm_(NULL), timeline_(NULL), profiles_(NULL), settings_(NULL), settings_seen_(0), language_seen_(0), compendium_(NULL), screen_node_(NULL),
      screen_(TITLE), chars_version_(0), decisions_seen_(0), starting_(false), in_run_(false),
      quit_(false), leaving_run_(false) {
    tutorials_ = NULL;
    tutorials_node_ = NULL;
    ascension_ = NULL;
    ascension_node_ = NULL;
    /* SettingsSave.Language (LocManager.Initialize); English when none */
    const Json saved = Loc::settings_save();
    if (saved["language"].is_string()) Loc::set_language(saved["language"].str());
    /* NGame.LaunchMainMenu: the logo first unless the settings skip it */
    if (!LogoScreen::skip_intro()) {
        std::string err;
        logo_ = new LogoScreen(lib);
        logo_node_ = logo_->build(&err);
        if (logo_node_) {
            tree_->root()->add_child(logo_node_);
        } else {
            delete logo_;
            logo_ = NULL;
        }
    }
    if (!logo_) show_title();
    /* The game data loads while the menu shows; the last profile is read
     * as the game does at start-up. */
    game_->request_characters();
    game_->request("{\"cmd\":\"select_profile\",\"profile\":0}");
}

MenuFlow::~MenuFlow() {
    if (logo_node_) {
        if (logo_node_->parent) logo_node_->parent->remove_child(logo_node_);
        delete logo_node_;
    }
    delete logo_;
    close_screen();
    if (title_node_) {
        if (title_node_->parent) title_node_->parent->remove_child(title_node_);
        delete title_node_;
    }
    title_->detach();
    delete title_;
    delete chars_;
}

void MenuFlow::note(const std::string& e) { event_ = event_.empty() ? e : event_ + "; " + e; }

std::string MenuFlow::take_event() {
    std::string e = event_;
    event_.clear();
    return e;
}

void MenuFlow::show_title() {
    std::string err;
    if (!title_node_) {
        title_node_ = title_->build(version_, &err);
        if (title_node_) tree_->root()->add_child(title_node_);
        else event_ = "main menu: " + err;
    }
    title_->set_menu(menu_);
    title_->set_covered(false);
    if (lib_->audio()) lib_->audio()->menu();
    tree_->layout();
}

/* NLanguageDropdown.OnDropdownItemSelected: NGame.Relocalize, then the
 * settings menu opened again. The title (behind) and an open settings
 * screen are built again in the new language; the menus' data (characters,
 * timeline, ...) is asked for again. */
void MenuFlow::relocalize() {
    std::string err;
    if (title_node_) {
        if (title_node_->parent) title_node_->parent->remove_child(title_node_);
        title_->detach();
        delete title_node_;
        title_node_ = title_->build(version_, &err);
        if (title_node_) {
            tree_->root()->add_child(title_node_);
            title_->set_menu(menu_);
        }
    }
    if (screen_ == SETTINGS && settings_) {
        if (screen_node_) {
            if (screen_node_->parent) screen_node_->parent->remove_child(screen_node_);
            delete screen_node_;
        }
        delete settings_;
        settings_ = new SettingsScreen(lib_);
        screen_node_ = settings_->build(game_->settings(), &err);
        settings_seen_ = game_->settings_version();
        if (screen_node_) tree_->root()->add_child(screen_node_);
    } else if (screen_node_) {
        /* the submenu stays over the title */
        tree_->root()->remove_child(screen_node_);
        tree_->root()->add_child(screen_node_);
    }
    if (title_node_ && screen_ != TITLE) {
        title_->set_covered(true);
        title_->set_submenu(true);
    }
    game_->send_query("{\"cmd\":\"main_menu\"}");
    tree_->layout();
}

void MenuFlow::close_ascension() {
    if (ascension_node_) {
        if (ascension_node_->parent) ascension_node_->parent->remove_child(ascension_node_);
        delete ascension_node_;
        ascension_node_ = NULL;
    }
    delete ascension_;
    ascension_ = NULL;
}

void MenuFlow::embark() {
    last_character_ = chars_->selected();
    note(format("run: %s, ascension %d", chars_->character().c_str(), chars_->ascension()));
    if (menu_.is_null()) game_->start_run(chars_->character(), format("%08X", chaotic_rand()));
    else game_->start_game_run(chars_->character(), chars_->ascension());
    begin_run();
}

/* The submenu over the title goes; the title takes the pad again. */
void MenuFlow::close_screen() {
    if (screen_node_) {
        if (screen_node_->parent) screen_node_->parent->remove_child(screen_node_);
        delete screen_node_;
        screen_node_ = NULL;
        if (screen_ == CHARACTERS) chars_->detach();
    }
    delete confirm_;
    confirm_ = NULL;
    if (tutorials_node_) {
        if (tutorials_node_->parent) tutorials_node_->parent->remove_child(tutorials_node_);
        delete tutorials_node_;
        tutorials_node_ = NULL;
    }
    delete tutorials_;
    tutorials_ = NULL;
    close_ascension();
    delete timeline_;
    timeline_ = NULL;
    delete profiles_;
    profiles_ = NULL;
    delete settings_;
    settings_ = NULL;
    delete compendium_;
    compendium_ = NULL;
    screen_ = TITLE;
    title_->set_covered(false);
    title_->set_submenu(false);
}

void MenuFlow::begin_run() {
    starting_ = true;
    if (screen_ == CHARACTERS) chars_->set_starting();
    title_->set_covered(true);
    decisions_seen_ = game_->decision_count();
}

void MenuFlow::open_timeline() {
    std::string err;
    timeline_ = new TimelineScreen(lib_);
    screen_node_ = timeline_->build(&err);
    if (!screen_node_) {
        event_ = "timeline: " + err;
        delete timeline_;
        timeline_ = NULL;
        return;
    }
    screen_ = TIMELINE;
    title_->set_covered(true);
    title_->set_submenu(true);
    tree_->root()->add_child(screen_node_);
    game_->request("{\"cmd\":\"timeline\"}");
}

void MenuFlow::handle_answer(const Json& a) {
    const std::string& type = a["type"].str();
    note("answer: " + type);
    if (type == "main_menu") {
        menu_ = a;
        title_->set_menu(menu_);
        chars_->set_progress(menu_);
        if (screen_ == ABANDON) close_screen();
        if (profiles_) profiles_->set_menu(menu_);
    } else if (compendium_ && CompendiumScreen::is_answer(type)) {
        compendium_->set_answer(a);
    } else if (type == "timeline") {
        if (timeline_) timeline_->set_data(a);
    } else if (type == "profiles") {
        if (profiles_) profiles_->set_profiles(a);
    } else if (type == "error") {
        note("logic: " + a["message"].str());
        /* No profile could be read: the menu without one. */
        if (menu_.is_null()) title_->set_menu(menu_);
        if (starting_) {
            /* The run could not start (a bad save): back to the menu. */
            starting_ = false;
            if (screen_ == CHARACTERS) close_screen();
            title_->set_covered(false);
            game_->request("{\"cmd\":\"main_menu\"}");
        }
    }
}

bool MenuFlow::update(const PadState& in) {
    PadState pad = in;
    /* The logic's answers to the menus. */
    Json answer;
    while (game_->take_answer(&answer)) handle_answer(answer);
    if (game_->language_version() != language_seen_) {
        language_seen_ = game_->language_version();
        if (!in_run_ && !logo_) relocalize();
    }
    if (game_->characters_version() != chars_version_) {
        chars_version_ = game_->characters_version();
        chars_->set_data(game_->characters());
    }

    if (logo_) {
        logo_->input(pad);
        if (logo_->done()) {
            logo_node_->parent->remove_child(logo_node_);
            delete logo_node_;
            logo_node_ = NULL;
            delete logo_;
            logo_ = NULL;
            show_title();
        }
        return !quit_;
    }

    if (in_run_) {
        game_->update(pad);
        if (game_->menu_requested()) {
            /* Back to the main menu: after a game over the run is let go
             * here (Save and Quit has done it already). */
            bool over = game_->game_over();
            /* NGameOverScreen.OnMainMenuButtonPressed: to the timeline
             * when the run discovered an epoch. */
            bool timeline = over && game_->decision()["progress"]["discovered_epochs"].size() > 0;
            game_->clear_screen();
            in_run_ = false;
            if (over) game_->request("{\"cmd\":\"return_to_menu\"}");
            close_screen();
            show_title();
            note("main menu");
            if (timeline) open_timeline();
        }
        return !quit_;
    }

    if (!starting_) {
        std::string err;
        switch (screen_) {
        case TITLE: {
            TitleScreen::Action a = title_->input(pad);
            if (a == TitleScreen::QUIT) quit_ = true;
            else if (a == TitleScreen::CONTINUE) {
                note("continue");
                game_->continue_run();
                begin_run();
            } else if (a == TitleScreen::ABANDON) {
                confirm_ = new ConfirmPopup(lib_);
                screen_node_ = confirm_->build(&err);
                if (screen_node_) {
                    screen_ = ABANDON;
                    title_->set_covered(true);
                    tree_->root()->add_child(screen_node_);
                } else {
                    delete confirm_;
                    confirm_ = NULL;
                }
            } else if (a == TitleScreen::SINGLEPLAYER) {
                screen_node_ = chars_->build(game_->characters(), last_character_, &err);
                if (screen_node_) {
                    screen_ = CHARACTERS;
                    chars_->set_progress(menu_);
                    title_->set_covered(true);
                    title_->set_submenu(true);
                    tree_->root()->add_child(screen_node_);
                } else {
                    event_ = "character select: " + err;
                }
            } else if (a == TitleScreen::TIMELINE) {
                open_timeline();
            } else if (a == TitleScreen::COMPENDIUM) {
                compendium_ = new CompendiumScreen(lib_);
                screen_node_ = compendium_->build("", &err);
                if (screen_node_) {
                    screen_ = COMPENDIUM;
                    title_->set_covered(true);
                    title_->set_submenu(true);
                    tree_->root()->add_child(screen_node_);
                    game_->send_query(CompendiumScreen::first_query());
                } else {
                    event_ = "compendium: " + err;
                    delete compendium_;
                    compendium_ = NULL;
                }
            } else if (a == TitleScreen::SETTINGS) {
                settings_ = new SettingsScreen(lib_);
                screen_node_ = settings_->build(game_->settings(), &err);
                if (screen_node_) {
                    screen_ = SETTINGS;
                    settings_seen_ = game_->settings_version();
                    title_->set_covered(true);
                    title_->set_submenu(true);
                    tree_->root()->add_child(screen_node_);
                } else {
                    event_ = "settings: " + err;
                    delete settings_;
                    settings_ = NULL;
                }
            } else if (a == TitleScreen::PROFILES) {
                profiles_ = new ProfileScreen(lib_);
                screen_node_ = profiles_->build(&err);
                if (screen_node_) {
                    screen_ = PROFILES;
                    profiles_->set_menu(menu_);
                    title_->set_covered(true);
                    title_->set_submenu(true);
                    tree_->root()->add_child(screen_node_);
                    game_->request("{\"cmd\":\"profiles\"}");
                } else {
                    event_ = "profiles: " + err;
                    delete profiles_;
                    profiles_ = NULL;
                }
            }
            break;
        }
        case CHARACTERS: {
            if (ascension_) {
                /* its "Got it!": MarkFtueAsComplete as it closes */
                if (ascension_->input(pad)) {
                    close_ascension();
                    game_->ftue_seen("ascension_singleplayer_ftue");
                }
                break;
            }
            if (!menu_.is_null() && game_->ftue_pending("ascension_singleplayer_ftue")) {
                ascension_ = new FtuePopup(lib_);
                ascension_node_ = ascension_->build("ascension_singleplayer_ftue", std::vector<CanvasItem*>(), NULL, &err);
                if (ascension_node_) {
                    tree_->root()->add_child(ascension_node_);
                    break;
                }
                delete ascension_;
                ascension_ = NULL;
                game_->ftue_seen("ascension_singleplayer_ftue");
            }
            if (tutorials_) {
                /* NAcceptTutorialsFtue: Yes keeps the tutorials, No turns
                 * them off; either way the Embark goes on */
                ConfirmPopup::Action t = tutorials_->input(pad);
                if (t == ConfirmPopup::NONE) break;
                game_->accept_tutorials(t == ConfirmPopup::YES);
                if (tutorials_node_->parent) tutorials_node_->parent->remove_child(tutorials_node_);
                delete tutorials_node_;
                tutorials_node_ = NULL;
                delete tutorials_;
                tutorials_ = NULL;
                embark();
                break;
            }
            CharacterSelectScreen::Action a = chars_->input(pad);
            if (a == CharacterSelectScreen::BACK) {
                last_character_ = chars_->selected();
                close_screen();
            } else if (a == CharacterSelectScreen::EMBARK) {
                if (!menu_.is_null() && game_->ftue_pending("accept_tutorials_ftue")) {
                    /* NCharacterSelectScreen.OnEmbarkPressed: the first time,
                     * whether to show the tutorials */
                    tutorials_ = new ConfirmPopup(lib_);
                    tutorials_node_ = tutorials_->build_scene("scenes/ftue/accept_tutorials_ftue.tscn",
                                                              Loc::get("main_menu_ui", "ENABLE_TUTORIALS.title"),
                                                              Loc::get("main_menu_ui", "ENABLE_TUTORIALS.description"), &err);
                    if (tutorials_node_) {
                        tree_->root()->add_child(tutorials_node_);
                        break;
                    }
                    delete tutorials_;
                    tutorials_ = NULL;
                }
                embark();
            }
            break;
        }
        case ABANDON: {
            ConfirmPopup::Action a = confirm_->input(pad);
            if (a == ConfirmPopup::YES) {
                /* NMainMenu.AbandonRun: the menu refreshes on the answer. */
                note("abandon run");
                game_->request("{\"cmd\":\"abandon_run\"}");
                set_visible(screen_node_, false);
            } else if (a == ConfirmPopup::NO) {
                close_screen();
            }
            break;
        }
        case TIMELINE: {
            std::string cmd;
            TimelineScreen::Action a = timeline_->input(pad, &cmd);
            if (!cmd.empty()) game_->request(cmd);
            if (a == TimelineScreen::BACK) {
                /* OnSubmenuHidden: the menu's buttons refresh. */
                close_screen();
                game_->request("{\"cmd\":\"main_menu\"}");
            }
            break;
        }
        case PROFILES: {
            std::string cmd;
            ProfileScreen::Action a = profiles_->input(pad, &cmd);
            if (!cmd.empty()) game_->request(cmd);
            if (a == ProfileScreen::BACK) close_screen();
            break;
        }
        case COMPENDIUM: {
            std::string cmd;
            CompendiumScreen::Action a = compendium_->input(pad, &cmd);
            if (!cmd.empty()) game_->send_query(cmd);
            if (a == CompendiumScreen::BACK) close_screen();
            break;
        }
        case SETTINGS: {
            std::string cmd;
            if (game_->settings_version() != settings_seen_) {
                settings_seen_ = game_->settings_version();
                settings_->set_settings(game_->settings());
            }
            SettingsScreen::Action a = settings_->input(pad, &cmd);
            if (!cmd.empty()) game_->send_query(cmd);
            if (a == SettingsScreen::BACK) close_screen();
            break;
        }
        }
    }

    game_->update(PadState());
    if (starting_ && game_->decision_count() != decisions_seen_) {
        /* The run's first screen: the menus go. */
        close_screen();
        if (title_node_) {
            if (title_node_->parent) title_node_->parent->remove_child(title_node_);
            delete title_node_;
            title_node_ = NULL;
            title_->detach();
        }
        starting_ = false;
        in_run_ = true;
    }
    return !quit_;
}

} /* namespace spire */
