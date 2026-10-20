#include "credits_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "loc.h"

#include <string.h>

namespace spire {

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> split(const std::string& s, const std::string& sep) {
    std::vector<std::string> out;
    size_t at = 0;
    for (;;) {
        size_t next = s.find(sep, at);
        out.push_back(s.substr(at, next == std::string::npos ? std::string::npos : next - at));
        if (next == std::string::npos) break;
        at = next + sep.size();
    }
    return out;
}

static std::string join(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += "\n";
        out += v[i];
    }
    return out;
}

/* SplitTwoColumn: "role || name" lines. */
static void two_columns(const std::string& in, std::string* roles, std::string* names) {
    std::vector<std::string> r, n;
    std::vector<std::string> lines = split(in, "\n");
    for (size_t i = 0; i < lines.size(); ++i) {
        if (trim(lines[i]).empty()) continue;
        std::vector<std::string> parts = split(lines[i], "||"), kept;
        for (size_t k = 0; k < parts.size(); ++k)
            if (!trim(parts[k]).empty()) kept.push_back(trim(parts[k]));
        if (kept.size() == 2) {
            r.push_back(kept[0]);
            n.push_back(kept[1]);
        }
    }
    *roles = join(r);
    *names = join(n);
}

/* SplitTwoColumnMultiRole: "role, role || name": one role a line, the name
 * on the first, a blank line after a multi-role entry (but the last). */
static void multi_role(const std::string& in, std::string* left, std::string* right) {
    std::vector<std::string> l, r;
    std::vector<std::string> lines, raw = split(in, "\n");
    for (size_t i = 0; i < raw.size(); ++i)
        if (!raw[i].empty()) lines.push_back(raw[i]);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::vector<std::string> parts = split(lines[i], "||");
        if (parts.size() != 2) continue;
        std::vector<std::string> roles = split(trim(parts[0]), ","), kept;
        for (size_t k = 0; k < roles.size(); ++k)
            if (!roles[k].empty()) kept.push_back(trim(roles[k]));
        for (size_t k = 0; k < kept.size(); ++k) {
            l.push_back(kept[k]);
            r.push_back(k == 0 ? trim(parts[1]) : std::string());
        }
        if (kept.size() > 1 && i + 1 != lines.size()) {
            l.push_back(std::string());
            r.push_back(std::string());
        }
    }
    *left = join(l);
    *right = join(r);
}

/* Rng.Chaotic's shuffle (ShuffleOneColumn, the playtesters). */
static void shuffle(std::vector<std::string>& v) {
    for (size_t i = v.size(); i-- > 1;) {
        size_t j = chaotic_rand() % (i + 1);
        std::swap(v[i], v[j]);
    }
}

CreditsScreen::CreditsScreen(SceneLibrary* lib)
    : lib_(lib), contents_(NULL), t_(0), target_(0), exiting_(false), exit_t_(0) {}

Node* CreditsScreen::build(std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/credits_screen.tscn");
    if (!screen) {
        if (error) *error = "no credits_screen.tscn";
        return NULL;
    }
    /* InitMegaCrit ... InitExitMessage */
    static const char* const plain[][2] = {
        { "MegaCritHeader", "MEGA_CRIT.header" }, { "CreatedByNames", "MEGA_CRIT.names" },
        { "ComposerHeader", "COMPOSER.header" }, { "ComposerNames", "COMPOSER.names" },
        { "AdditionalProgrammingHeader", "ADDITIONAL_PROGRAMMING.header" },
        { "AdditionalProgrammingNames", "ADDITIONAL_PROGRAMMING.names" },
        { "AdditionalVfxHeader", "ADDITIONAL_VFX.header" }, { "AdditionalVfxNames", "ADDITIONAL_VFX.names" },
        { "MarketingSupportHeader", "MARKETING_SUPPORT.header" }, { "MarketingSupportNames", "MARKETING_SUPPORT.names" },
        { "ConsultantsHeader", "CONSULTANTS.header" }, { "ConsultantsNames", "CONSULTANTS.names" },
        { "VoicesHeader", "VOICES.header" }, { "LocalizationHeader", "LOC.header" },
        { "ptbHeader", "LOC_PTB.header" }, { "ptbNames", "LOC_PTB.names" }, { "zhsHeader", "LOC_ZHS.header" },
        { "zhtHeader", "LOC_ZHT.header" }, { "zhtNames", "LOC_ZHT.names" }, { "fraHeader", "LOC_FRA.header" },
        { "deuHeader", "LOC_DEU.header" }, { "deuNames", "LOC_DEU.names" }, { "indHeader", "LOC_IND.header" },
        { "indNames", "LOC_IND.names" }, { "itaHeader", "LOC_ITA.header" }, { "itaTeam", "LOC_ITA.team" },
        { "jpnHeader", "LOC_JPN.header" }, { "jpnNames", "LOC_JPN.names" }, { "korHeader", "LOC_KOR.header" },
        { "korNames", "LOC_KOR.names" }, { "polHeader", "LOC_POL.header" }, { "polNames", "LOC_POL.names" },
        { "rusHeader", "LOC_RUS.header" }, { "spaHeader", "LOC_SPA.header" }, { "spaNames", "LOC_SPA.names" },
        { "espHeader", "LOC_ESP.header" }, { "espTeam", "LOC_ESP.team" }, { "thaHeader", "LOC_THA.header" },
        { "thaNames", "LOC_THA.names" }, { "turHeader", "LOC_TUR.header" }, { "TwitchHeader", "TWITCH.header" },
        { "ModdingSupportHeader", "MODDING_SUPPORT.header" }, { "PlaytestersHeader", "PLAYTESTERS.header" },
        { "TrailerHeader", "TRAILER.header" }, { "TrailerAnimationTeam", "TRAILER_ANIMATION.team" },
        { "TrailerAnimationHeader", "TRAILER_ANIMATION.header" }, { "TrailerEditorHeader", "TRAILER_EDITOR.header" },
        { "FmodHeader", "FMOD" }, { "SpineHeader", "SPINE" }, { "GodotHeader", "GODOT" }, { "ExitMessage", "EXIT_MESSAGE" } };
    for (size_t i = 0; i < sizeof(plain) / sizeof(plain[0]); ++i)
        set_label(screen->find_unique(plain[i][0]), Loc::get("credits", plain[i][1]));
    static const char* const pairs[][3] = {
        { "MegaCritTeamRoles", "MegaCritTeamNames", "MEGA_CRIT_TEAM.names" }, { "zhsRoles", "zhsNames", "LOC_ZHS.names" },
        { "fraRoles", "fraNames", "LOC_FRA.names" }, { "itaRoles", "itaNames", "LOC_ITA.names" },
        { "rusRoles", "rusNames", "LOC_RUS.names" }, { "espRoles", "espNames", "LOC_ESP.names" },
        { "turRoles", "turNames", "LOC_TUR.names" }, { "TwitchRoles", "TwitchNames", "TWITCH.names" },
        { "TrailerAnimationRoles", "TrailerAnimationNames", "TRAILER_ANIMATION.names" },
        { "TrailerEditorRoles", "TrailerEditorNames", "TRAILER_EDITOR.names" } };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        std::string roles, names;
        two_columns(Loc::get("credits", pairs[i][2]), &roles, &names);
        set_label(screen->find_unique(pairs[i][0]), roles);
        set_label(screen->find_unique(pairs[i][1]), names);
    }
    std::string voices_l, voices_r;
    multi_role(Loc::get("credits", "VOICES.names"), &voices_l, &voices_r);
    set_label(screen->find_unique("VoicesRoles"), voices_l);
    set_label(screen->find_unique("VoicesNames"), voices_r);
    std::vector<std::string> modding = split(Loc::get("credits", "MODDING_SUPPORT.names"), "||");
    shuffle(modding);
    set_label(screen->find_unique("ModdingSupportNames"), join(modding));
    /* SplitThreeColumnPlaytesters */
    std::vector<std::string> raw = split(Loc::get("credits", "PLAYTESTERS.names"), "||"), testers, cols[3];
    for (size_t i = 0; i < raw.size(); ++i)
        if (!trim(raw[i]).empty()) testers.push_back(trim(raw[i]));
    shuffle(testers);
    for (size_t i = 0; i < testers.size(); ++i) cols[i % 3].push_back(testers[i]);
    set_label(screen->find_unique("PlaytesterNames1"), join(cols[0]));
    set_label(screen->find_unique("PlaytesterNames2"), join(cols[1]));
    set_label(screen->find_unique("PlaytesterNames3"), join(cols[2]));
    show_glyph(lib_, find_named(screen->get_node("BackButton"), "HotkeyIcon"), "b");

    Node* c = screen->find_unique("ScreenContents");
    contents_ = c && c->is_control() ? (Control*)c : NULL;
    if (contents_) {
        contents_->modulate = Color(1, 1, 1, 0);
        tweens_.tween(&contents_->modulate.a, 1.0f, 2.0f);
        target_ = contents_->offset[SIDE_TOP];
    }
    screen->add_child(frame_driver(this));
    return screen;
}

bool CreditsScreen::input(const PadState& pad) {
    if (exiting_) return exit_t_ < 1.0;
    if (contents_ && (pad.pressed & PadState::UP)) target_ += 50;
    if (contents_ && (pad.pressed & PadState::DOWN)) target_ -= 50;
    if ((pad.pressed & (PadState::B | PadState::A)) && t_ >= 2.0) {
        /* CloseScreenDebug -> FadeAndExitScreen */
        exiting_ = true;
        exit_t_ = 0;
        if (contents_) {
            tweens_.kill(&contents_->modulate.a);
            tweens_.tween(&contents_->modulate.a, 0.0f, 1.0f);
        }
    }
    return true;
}

void CreditsScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    if (exiting_) exit_t_ += dt;
    if (!contents_) return;
    /* _Process: the target 80 px a second higher, the contents lerping */
    float d = (float)dt;
    target_ -= 80.0f * d;
    float h = contents_->offset[SIDE_BOTTOM] - contents_->offset[SIDE_TOP];
    float y = contents_->offset[SIDE_TOP];
    float k = d * 20.0f;
    y += (target_ - y) * (k > 1 ? 1 : k);
    contents_->offset[SIDE_TOP] = y;
    contents_->offset[SIDE_BOTTOM] = y + h;
}

} /* namespace spire */
