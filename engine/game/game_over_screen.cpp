#include "game_over_screen.h"
#include "room_cues.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"


namespace spire {

/* NGameOverScreen.AnimateIn: the backstop's transition (threshold 0 -> 1,
 * 1.5 s; its material's alpha 0.9), then the banner and the Ui (0.25 s). */
static const float BACKSTOP_TIME = 1.5f, UI_FADE = 0.25f, BACKSTOP_ALPHA = 0.9f;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

/* A random entry of a loc table's numbered keys (prefix0, prefix1, ... or
 * prefix00, prefix01, ...): LocTable.GetLocStringsWithPrefix + a random pick. */
static std::string random_loc(const char* table, const char* prefix, bool two_digits) {
    std::vector<std::string> found;
    for (int i = 0; i < 100; ++i) {
        std::string key = format(two_digits ? "%s%02d" : "%s%d", prefix, i);
        std::string text = Loc::get(table, key.c_str());
        if (text == std::string(table) + "." + key) break;
        found.push_back(text);
    }
    if (found.empty()) return std::string();
    return found[(size_t)(chaotic_rand() % (unsigned)found.size())];
}

GameOverScreen::GameOverScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), backstop_(NULL), ui_(NULL), banner_(NULL), quote_(NULL), victory_(NULL),
      summary_backstop_(NULL), summary_(NULL), continue_(NULL), main_menu_(NULL), phase_(INTRO), t_(0), step_(0), lines_started_(0),
      continue_on_(false), menu_on_(false), done_(false), bar_(NULL), bar_fg_(NULL), bar_score_(0), bar_threshold_(0),
      bar_t_(0), bar_step_(0), badges_started_(0), discoveries_started_(0),
      discoveries_t_(-1), badge_focus_(-1), badge_tip_(NULL), root_(NULL), screen_(NULL) {}

Node* GameOverScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    Control* root = full_rect_control("GameOverOverlay");
    if (background_) {
        root->add_child(background_);
        background_ = NULL;
    }
    Node* screen = instance(lib_, "scenes/screens/game_over_screen.tscn");
    if (!screen) {
        if (error) *error = "no game_over_screen.tscn";
        delete root;
        return NULL;
    }
    root->add_child(screen);
    root_ = root;
    screen_ = screen;
    hide(screen->find_unique("BadgeContainer"));
    hide(screen->find_unique("DailyRunLeaderboard"));
    hide(screen->find_unique("RunMocker"));
    hide(find_named(screen, "TestModeLabel"));
    hide(screen->find_unique("LeaderboardButton"));
    hide(screen->find_unique("ViewRunButton"));
    /* the badges: shown on the summary (AnimateBadges) */
    bar_ = ctl(screen, "ScoreBar");
    bar_fg_ = ctl(screen, "ScoreFg");
    bar_step_ = 0;
    /* No progress (the test flow) or nothing left to unlock: no bar. */
    if (!d["progress"]["score_bar"].is_object() || d["progress"]["score_bar"]["unlocks_remaining"].i() <= 0) {
        hide(bar_);
        bar_ = NULL;
    }
    /* NRunSummary._Ready: the discovered items hidden, the header out */
    static const char* const disc[5] = { "DiscoveredCards", "DiscoveredRelics", "DiscoveredPotions", "DiscoveredEnemies",
                                         "DiscoveredEpochs" };
    for (int k = 0; k < 5; ++k) hide(screen->find_unique(disc[k]));
    if (Control* dh = ctl(screen, "DiscoveryHeader")) dh->modulate.a = 0;

    /* The backstop's wipe (texture_transition.gdshader) as a fade. */
    backstop_ = ctl(screen, "Backstop");
    if (backstop_) {
        backstop_->material = NULL;
        backstop_->modulate.a = 0;
        tweens_.tween(&backstop_->modulate.a, BACKSTOP_ALPHA, BACKSTOP_TIME, TRANS_SINE, EASE_IN_OUT);
    }
    ui_ = ctl(screen, "Ui");
    if (ui_) ui_->modulate = Color(1, 1, 1, 0);
    summary_backstop_ = ctl(screen, "SummaryBackstop");
    summary_ = ctl(screen, "RunSummaryContainer");
    if (summary_) summary_->visible = false;

    bool won = d["victory"].b();
    banner_ = ctl(screen, "Banner");
    if (banner_) {
        set_label(banner_->find_unique("Label"),
                  won ? Loc::get("game_over_screen", "BANNER.falseWin") : random_loc("game_over_screen", "BANNER.lose", false));
        banner_->modulate.a = 0;
    }
    quote_ = ctl(screen, "DeathQuoteLabel");
    if (quote_) set_label(quote_, won ? std::string() : random_loc("game_over_screen", "QUOTES.", true));
    victory_ = ctl(screen, "VictoryDamageLabel");
    if (victory_) {
        set_label(victory_, won ? d["victory_text"].str() : std::string());
        if (victory_->type == "RichTextLabel") {
            ((RichTextLabel*)victory_)->visible_characters = -1;
            ((RichTextLabel*)victory_)->visible_ratio = 0;
        }
    }

    continue_ = ctl(screen, "ContinueButton");
    if (continue_) {
        set_label(continue_->get_node("Label"), Loc::get("game_over_screen", "BUTTON.continue"));
        show_glyph(lib_, continue_->find_unique("HotkeyIcon"), "a");
        continue_->modulate = Color(0, 0, 0, 0);
    }
    main_menu_ = ctl(screen, "MainMenuButton");
    if (main_menu_) {
        set_label(main_menu_->get_node("Label"), Loc::get("game_over_screen", "BUTTON.mainMenu"));
        show_glyph(lib_, main_menu_->find_unique("HotkeyIcon"), "a");
        /* NReturnToMainMenuButton._Ready: 140 px left of the scene's place
         * (the View Run button beside it is not shown). */
        main_menu_->offset[SIDE_LEFT] -= 140;
        main_menu_->offset[SIDE_RIGHT] -= 140;
        main_menu_->visible = false;
    }
    root->add_child(frame_driver(this));
    return root;
}

/* NGameOverContinueButton.OnEnable (from 190 px below, Quart Out) or
 * NReturnToMainMenuButton's (in place), both fading in; focused at once
 * (the pad's: scale 1.1, hsv s 1.2 v 1.4). */
void GameOverScreen::enable_button(Control* b, bool from_below) {
    if (!b) return;
    b->visible = true;
    if (from_below) {
        float top = b->offset[SIDE_TOP], bottom = b->offset[SIDE_BOTTOM];
        tweens_.tween_from(&b->offset[SIDE_TOP], top + 190, top, 0.5f, TRANS_QUART, EASE_OUT);
        tweens_.tween_from(&b->offset[SIDE_BOTTOM], bottom + 190, bottom, 0.5f, TRANS_QUART, EASE_OUT);
    }
    b->modulate = Color(0, 0, 0, 0);
    tweens_.tween(&b->modulate.r, 1.0f, 0.5f);
    tweens_.tween(&b->modulate.g, 1.0f, 0.5f);
    tweens_.tween(&b->modulate.b, 1.0f, 0.5f);
    tweens_.tween(&b->modulate.a, 1.0f, 0.5f);
    b->pivot_offset = offset_size(b) * 0.5f;
    tweens_.tween(&b->scale, Vec2(1.1f, 1.1f), 0.05f);
    Control* image = as_control(b->get_node("Image"));
    if (image) {
        tweens_.tween(&image->shader_params["s"], 1.2f, 0.05f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&image->shader_params["v"], 1.4f, 0.05f, TRANS_EXPO, EASE_OUT);
    }
}

/* AnimateInQuote: position:y 90 -> 156 (2 s, Expo Out) in the banner,
 * alpha to 1 (1.5 s). The label hangs from the banner's centre. */
void GameOverScreen::animate_quote(float delay) {
    if (!quote_ || !banner_) return;
    float centre = (banner_->offset[SIDE_BOTTOM] - banner_->offset[SIDE_TOP]) * 0.5f;
    float h = quote_->offset[SIDE_BOTTOM] - quote_->offset[SIDE_TOP];
    tweens_.tween_from(&quote_->offset[SIDE_TOP], 90 - centre, 156 - centre, 2.0f, TRANS_EXPO, EASE_OUT, delay);
    tweens_.tween_from(&quote_->offset[SIDE_BOTTOM], 90 - centre + h, 156 - centre + h, 2.0f, TRANS_EXPO, EASE_OUT,
                       delay);
    tweens_.tween(&quote_->modulate.a, 1.0f, 1.5f, TRANS_LINEAR, EASE_IN_OUT, delay);
}

/* NScoreLine (score_line.tscn: %Icon, %Label, %Score); a plain row if the
 * pack lacks the scene. */
Node* GameOverScreen::score_line(const Json& line) {
    std::string icon = line["icon"].str();
    const char* scene = "scenes/screens/game_over_screen/score_line.tscn";
    Node* n = lib_->file(scene) ? instance(lib_, scene) : NULL;
    if (n) {
        set_label(n->find_unique("Label"), line["text"].str());
        set_label(n->find_unique("Score"), line["score"].str());
        set_texture(lib_, n->find_unique("Icon"), icon);
        return n;
    }
    Control* row = (Control*)SceneLibrary::create_node("HBoxContainer");
    row->custom_minimum_size = Vec2(560, 56);
    TextureRect* t = (TextureRect*)SceneLibrary::create_node("TextureRect");
    t->custom_minimum_size = Vec2(48, 48);
    t->expand_mode = TextureRect::EXPAND_IGNORE_SIZE;
    t->stretch_mode = TextureRect::STRETCH_KEEP_ASPECT_CENTERED;
    set_texture(lib_, t, icon);
    row->add_child(t);
    Label* label = (Label*)SceneLibrary::create_node("Label");
    label->set_text(line["text"].str());
    label->size_flags_h = SIZE_FILL | SIZE_EXPAND;
    label->add_theme_font_size_override("font_size", 30);
    label->add_theme_color_override("font_color", hex(0xFFF6E2));
    row->add_child(label);
    Label* score = (Label*)SceneLibrary::create_node("Label");
    score->set_text(line["score"].str());
    score->add_theme_font_size_override("font_size", 30);
    score->add_theme_color_override("font_color", hex(0xEFC851));
    row->add_child(score);
    return row;
}

void GameOverScreen::set_score_text(float value) {
    bar_score_ = value;
    if (!bar_) return;
    set_label(bar_->find_unique("ScoreProgress"), format("[%d/%d]", (int)(value + 0.5f), bar_threshold_));
}

/* AnimateScoreBar's start: the unlocks left, the score so far. */
void GameOverScreen::start_score_bar() {
    const Json& b = decision_["progress"]["score_bar"];
    bar_threshold_ = b["threshold"].i();
    set_label(bar_->find_unique("UnlocksRemaining"),
              Loc::format("game_over_screen", "SCORE.unlocksRemaining", "UnlockCount", format("%d", b["unlocks_remaining"].i())));
    set_score_text((float)b["current"].i());
    if (bar_fg_) bar_fg_->scale.x = bar_threshold_ > 0 ? (float)b["current"].i() / (float)bar_threshold_ : 0;
    tweens_.tween(&bar_->modulate.a, 1.0f, 0.3f);
    bar_t_ = 0;
    bar_step_ = 1;
}

/* NBadge.OnFocus / OnUnfocus: the reticle, the tip 10 px right and 132 px
 * above it. */
void GameOverScreen::focus_badge(int i) {
    if (badge_focus_ >= 0 && badge_focus_ < (int)focusables_.size())
        hide(focusables_[(size_t)badge_focus_]->find_unique("SelectionReticle"));
    if (badge_tip_) {
        if (badge_tip_->parent) badge_tip_->parent->remove_child(badge_tip_);
        delete badge_tip_;
        badge_tip_ = NULL;
    }
    badge_focus_ = i;
    if (i < 0 || i >= (int)focusables_.size()) return;
    Control* b = focusables_[(size_t)i];
    set_visible(b->find_unique("SelectionReticle"), true);
    const Json& d = *focus_tips_[(size_t)i];
    badge_tip_ = make_hover_tip(lib_, d["title"].str(), d["description"].str());
    if (badge_tip_ && root_) {
        badge_tip_->z_index = 10;
        Vec2 at = b->global_transform().origin + Vec2(10, -132);
        badge_tip_->set_position(at);
        root_->add_child(badge_tip_);
    }
}

std::string GameOverScreen::input(const PadState& pad) {
    if (phase_ == SUMMARY && menu_on_ && !focusables_.empty() && (pad.pressed & (PadState::LEFT | PadState::RIGHT))) {
        /* the badges' and discoveries' focus neighbours wrap left and right */
        int n = (int)focusables_.size();
        int i = badge_focus_ < 0 ? 0 : (badge_focus_ + ((pad.pressed & PadState::LEFT) ? n - 1 : 1)) % n;
        focus_badge(i);
        return std::string();
    }
    if (!(pad.pressed & PadState::A)) return std::string();
    if (phase_ == TITLE && continue_on_) {
        /* OpenSummaryScreen. */
        continue_on_ = false;
        if (continue_) continue_->visible = false;
        if (victory_) victory_->visible = false;
        phase_ = SUMMARY;
        t_ = 0;
        step_ = 0;
        if (summary_backstop_) {
            tweens_.tween(&summary_backstop_->modulate.a, 1.0f, 0.5f);
        }
        /* AnimateInQuote again: out (0.25 s), the encounter's quote, a
         * second's wait, in. */
        if (quote_ && !decision_["victory"].b()) {
            tweens_.tween(&quote_->modulate.a, 0.0f, 0.25f);
        }
        /* The banner 32 px up (0.5 s, Cubic Out). */
        if (banner_) {
            tweens_.tween(&banner_->offset[SIDE_TOP], banner_->offset[SIDE_TOP] - 32, 0.5f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&banner_->offset[SIDE_BOTTOM], banner_->offset[SIDE_BOTTOM] - 32, 0.5f, TRANS_CUBIC, EASE_OUT);
        }
        /* AnimateBadges: each NBadge.Create, hidden until its AnimateIn */
        Node* bc = screen_ ? screen_->find_unique("BadgeContainer") : NULL;
        const Json& bl = decision_["progress"]["badges"];
        set_visible(bc, bl.size() > 0);
        for (size_t i = 0; bc && i < bl.size(); ++i) {
            Control* b = as_control(instance(lib_, "scenes/screens/game_over_screen/badge.tscn"));
            if (!b) continue;
            set_texture(lib_, b->find_unique("BadgeHolder"), bl[i]["base"].str());
            set_texture(lib_, b->find_unique("Icon"), bl[i]["icon"].str());
            hide(b->find_unique("SelectionReticle"));
            b->modulate.a = 0;
            bc->add_child(b);
            badges_.push_back(b);
            focusables_.push_back(b);
            focus_tips_.push_back(&bl[i]);
        }
        const Json& dl = decision_["progress"]["discoveries"];
        for (size_t i = 0; i < dl.size(); ++i) {
            Control* item = screen_ ? ctl(screen_, dl[i]["node"].str().c_str()) : NULL;
            if (!item) continue;
            set_label(item->find_unique("Label"), format("%d", dl[i]["count"].i()));
            hide(item->find_unique("SelectionReticle"));
            discoveries_.push_back(item);
        }
        if (summary_) {
            summary_->visible = true;
            Node* grid = summary_->find_unique("ScoreLineContainer");
            const Json& ls = decision_["score_lines"];
            for (size_t i = 0; grid && i < ls.size(); ++i) {
                Node* n = score_line(ls[i]);
                Control* c = as_control(n);
                if (c) c->modulate.a = 0;
                grid->add_child(n);
                if (c) lines_.push_back(c);
            }
        }
        return std::string();
    }
    if (phase_ == SUMMARY && menu_on_) {
        menu_on_ = false;
        done_ = true;
    }
    return std::string();
}

void GameOverScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    if (phase_ == INTRO && t_ >= BACKSTOP_TIME) {
        /* NCommonBanner.AnimateIn and the Ui's fade; then the quote and
         * Continue. */
        phase_ = TITLE;
        t_ = 0;
        if (banner_) {
            tweens_.tween(&banner_->modulate.a, 1.0f, 0.4f, TRANS_EXPO, EASE_OUT);
            float top = banner_->offset[SIDE_TOP], bottom = banner_->offset[SIDE_BOTTOM];
            tweens_.tween_from(&banner_->offset[SIDE_TOP], top + 50, top, 0.4f, TRANS_BACK, EASE_OUT);
            tweens_.tween_from(&banner_->offset[SIDE_BOTTOM], bottom + 50, bottom, 0.4f, TRANS_BACK, EASE_OUT);
        }
        if (ui_) tweens_.tween(&ui_->modulate.a, 1.0f, UI_FADE);
    }
    if (phase_ == TITLE && step_ == 0 && t_ >= UI_FADE) {
        step_ = 1;
        if (decision_["victory"].b() && victory_) {
            tweens_.tween(&((RichTextLabel*)victory_)->visible_ratio, 1.0f, 2.0f, TRANS_SINE, EASE_OUT);
            tweens_.tween(&victory_->modulate.a, 1.0f, 2.0f);
        } else {
            animate_quote(0);
        }
        enable_button(continue_, true);
        continue_on_ = true;
    }
    if (phase_ == SUMMARY) {
        /* The encounter's quote after the old one is out and a second. */
        if (step_ == 0 && t_ >= 1.25) {
            step_ = 1;
            if (quote_ && !decision_["victory"].b() && decision_["death_quote"].is_string()) {
                set_label(quote_, decision_["death_quote"].str());
                animate_quote(0);
            }
        }
        /* The score lines one by one (0.3 s each and 0.1 s between), half
         * a second; then the badges, the score bar and the discoveries. */
        float line_t = 0.4f;
        while (lines_started_ < lines_.size() && t_ >= line_t * (float)lines_started_) {
            tweens_.tween(&lines_[lines_started_]->modulate.a, 1.0f, 0.3f);
            ++lines_started_;
        }
        float lines_end = line_t * (float)lines_.size() + 0.5f;
        /* AnimateBadges: a quarter second, each badge in (0.25 s: alpha,
         * up from 40 px below), half a second */
        while (badges_started_ < badges_.size() && t_ >= lines_end + 0.25f + 0.25f * (float)badges_started_) {
            Control* b = badges_[badges_started_];
            tweens_.tween(&b->modulate.a, 1.0f, 0.25f);
            Control* holder = as_control(b->find_unique("BadgeHolder"));
            if (holder) {
                tweens_.tween_from(&holder->offset[SIDE_TOP], holder->offset[SIDE_TOP] + 40, holder->offset[SIDE_TOP], 0.25f, TRANS_SPRING, EASE_OUT);
                tweens_.tween_from(&holder->offset[SIDE_BOTTOM], holder->offset[SIDE_BOTTOM] + 40, holder->offset[SIDE_BOTTOM], 0.25f, TRANS_SPRING, EASE_OUT);
            }
            ++badges_started_;
        }
        lines_end += 0.75f + 0.25f * (float)badges_.size();
        if (step_ == 1 && bar_ && t_ >= lines_end && bar_step_ == 0) start_score_bar();
        if (bar_step_ > 0 && bar_step_ < 4) {
            bar_t_ += dt;
            const Json& b = decision_["progress"]["score_bar"];
            float score = (float)decision_["progress"]["score"].i();
            float cur = (float)b["current"].i();
            float threshold = (float)(bar_threshold_ > 0 ? bar_threshold_ : 1);
            if (bar_step_ == 1 && bar_t_ >= 0.3) {
                bar_step_ = 2;
                bar_t_ = 0;
                if (!b["unlocked"].b()) {
                    /* Not enough for an unlock: the score counts up (1 s
                     * after 0.5 s). */
                    if (bar_fg_) tweens_.tween(&bar_fg_->scale.x, (cur + score) / threshold, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 0.5f);
                } else {
                    /* The bar fills (1 s after a second, Cubic Out). */
                    if (bar_fg_) tweens_.tween(&bar_fg_->scale.x, 1.0f, 1.0f, TRANS_CUBIC, EASE_OUT, 1.0f);
                    set_label(bar_->find_unique("UnlockText"),
                              Loc::get("game_over_screen", b["next_threshold"].i() == 0 ? "SCORE.unlockedAllMessage"
                                                                                        : "SCORE.unlockedEpochMessage"));
                }
            } else if (bar_step_ == 2 && !b["unlocked"].b()) {
                float k = bar_t_ < 0.5 ? 0.0f : bar_t_ > 1.5 ? 1.0f : (float)(bar_t_ - 0.5);
                set_score_text(cur + score * k);
                if (bar_t_ >= 1.5) bar_step_ = 4;
            } else if (bar_step_ == 2) {
                float k = bar_t_ < 1.0 ? 0.0f : bar_t_ > 2.0 ? 1.0f : (float)(bar_t_ - 1.0);
                k = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);
                set_score_text(cur + score + ((float)bar_threshold_ - cur - score) * k);
                if (bar_t_ >= 2.0) {
                    /* The unlock: the text springs up (0.25 s), the bar
                     * starts over towards the next unlock. */
                    bar_step_ = 3;
                    bar_t_ = 0;
                    /* SaveManager.GrantNextUnlock: NGainEpochVfx */
                    show_epoch_gain(lib_, root_, b["epoch"].str());
                    Control* text = as_control(bar_->find_unique("UnlockText"));
                    if (text) {
                        tweens_.tween(&text->modulate.a, 1.0f, 0.25f);
                        float h = text->offset[SIDE_BOTTOM] - text->offset[SIDE_TOP];
                        tweens_.tween(&text->offset[SIDE_TOP], -60.0f - h * 0.5f, 0.25f, TRANS_SPRING, EASE_OUT);
                        tweens_.tween(&text->offset[SIDE_BOTTOM], -60.0f + h * 0.5f, 0.25f, TRANS_SPRING, EASE_OUT);
                    }
                    set_label(bar_->find_unique("UnlocksRemaining"),
                              Loc::format("game_over_screen", "SCORE.unlocksRemaining", "UnlockCount",
                                          format("%d", b["unlocks_remaining"].i() - 1)));
                    bar_threshold_ = b["next_threshold"].i();
                    if (bar_threshold_ > 0 && bar_fg_) {
                        bar_fg_->scale.x = 0;
                        tweens_.tween(&bar_fg_->scale.x, (float)b["after"].i() / (float)bar_threshold_, 1.0f, TRANS_CUBIC,
                                      EASE_OUT, 0.5f);
                    }
                    set_score_text(0);
                }
            } else if (bar_step_ == 3) {
                float k = bar_t_ < 0.5 ? 0.0f : bar_t_ > 1.5 ? 1.0f : (float)(bar_t_ - 0.5);
                k = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);
                if (bar_threshold_ > 0) set_score_text((float)b["after"].i() * k);
                if (bar_t_ >= 1.5) bar_step_ = 4;
            }
        }
        /* AnimateInDiscoveries: the header (0.25 s), 0.1 s, then each item
         * in turn (0.3 s: white, up from 100 px, Back Out) */
        if (step_ == 1 && discoveries_t_ < 0 && (bar_ ? bar_step_ == 4 : t_ >= lines_end)) {
            discoveries_t_ = t_;
            if (!discoveries_.empty()) {
                if (Control* dh = ctl(screen_, "DiscoveryHeader")) tweens_.tween(&dh->modulate.a, 1.0f, 0.25f);
            }
        }
        while (discoveries_t_ >= 0 && discoveries_started_ < discoveries_.size() &&
               t_ >= discoveries_t_ + 0.1 + 0.3 * (double)discoveries_started_) {
            Control* item = discoveries_[discoveries_started_];
            item->visible = true;
            item->modulate = Color(0, 0, 0, 0);
            tweens_.tween(&item->modulate.r, 1.0f, 0.3f);
            tweens_.tween(&item->modulate.g, 1.0f, 0.3f);
            tweens_.tween(&item->modulate.b, 1.0f, 0.3f);
            tweens_.tween(&item->modulate.a, 1.0f, 0.3f);
            Control* inner = as_control(item->get_node("HBoxContainer"));
            if (inner) {
                tweens_.tween_from(&inner->offset[SIDE_TOP], inner->offset[SIDE_TOP] + 100, inner->offset[SIDE_TOP], 0.3f, TRANS_BACK, EASE_OUT);
                tweens_.tween_from(&inner->offset[SIDE_BOTTOM], inner->offset[SIDE_BOTTOM] + 100, inner->offset[SIDE_BOTTOM], 0.3f, TRANS_BACK, EASE_OUT);
            }
            focusables_.push_back(item);
            const Json& dl = decision_["progress"]["discoveries"];
            for (size_t k = 0; k < dl.size(); ++k)
                if (ctl(screen_, dl[k]["node"].str().c_str()) == item) focus_tips_.push_back(&dl[k]);
            ++discoveries_started_;
        }
        if (step_ == 1 && discoveries_t_ >= 0 && t_ >= discoveries_t_ + (discoveries_.empty() ? 0.0 : 0.1 + 0.3 * (double)discoveries_.size())) {
            step_ = 2;
            enable_button(main_menu_, false);
            menu_on_ = true;
        }
    }
}

} /* namespace spire */
