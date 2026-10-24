#include "card_view.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "loc.h"
#include "ui_util.h"
#include "../scene/script.h"
#include "../scene/tween.h"

#include <ctype.h>

namespace spire {

static std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

static Color hexa(unsigned v, float a = 1.0f) {
    return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, a);
}

static const Color CREAM(1, 0.964706f, 0.886275f);

static CanvasItem* item(Node* scope, const char* unique) {
    Node* n = scope->find_unique(unique);
    if (!n) n = scope->get_node(std::string("CardContainer/") + unique);
    return n && n->is_canvas_item() ? (CanvasItem*)n : NULL;
}

void warn_missing(const std::string& path); /* ui_util.h */

static void set_texture(SceneLibrary* lib, CanvasItem* n, const std::string& path) {
    if (!n) return;
    Resource* r = lib->ext(path);
    Texture2DRes* t = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    if (!t && !path.empty()) warn_missing(path);
    if (n->type == "TextureRect") ((TextureRect*)n)->texture = t;
    else if (n->type == "NinePatchRect") ((NinePatchRect*)n)->texture = t;
}

static void set_material(SceneLibrary* lib, CanvasItem* n, const std::string& path) {
    if (!n) return;
    Resource* r = lib->ext(path);
    n->material = r && (r->kind == Resource::RES_SHADER_MATERIAL || r->kind == Resource::RES_CANVAS_ITEM_MATERIAL)
                      ? (MaterialRes*)r
                      : NULL;
}

static void set_text(CanvasItem* n, const std::string& s) {
    if (n && n->type == "Label") ((Label*)n)->set_text(s);
    else if (n && n->type == "RichTextLabel") ((RichTextLabel*)n)->text = s;
}

static void colors(CanvasItem* n, const Color& font, const Color& outline) {
    if (!n || !n->is_control()) return;
    ((Control*)n)->add_theme_color_override("font_color", font);
    ((Control*)n)->add_theme_color_override("font_outline_color", outline);
}

/* CardRarity -> the banner material (card_banner_<x>_mat) and the title
 * outline (StsColors.cardTitleOutline*). */
static void rarity_style(const std::string& rarity, std::string& banner, Color& outline) {
    banner = "common";
    outline = hexa(0x4D4B40);
    if (rarity == "Uncommon") {
        banner = "uncommon";
        outline = hexa(0x005C75);
    } else if (rarity == "Rare") {
        banner = "rare";
        outline = hexa(0x6B4B00);
    } else if (rarity == "Curse") {
        banner = "curse";
        outline = hexa(0x550B9E);
    } else if (rarity == "Status") {
        banner = "status";
        outline = hexa(0x4F522F);
    } else if (rarity == "Event") {
        banner = "event";
        outline = hexa(0x1B6131);
    } else if (rarity == "Quest") {
        banner = "quest";
        outline = hexa(0x7E3E15);
    } else if (rarity == "Ancient") {
        banner = "ancient";
    }
}

/* CardCostHelper colours: Increased, Decreased, InsufficientResources. */
static bool cost_color(const std::string& state, bool just_upgraded, Color& font, Color& outline) {
    if (just_upgraded || state == "Decreased") {
        font = hexa(0x7FFF00);
        outline = hexa(0x1F5923);
    } else if (state == "Increased") {
        font = hexa(0x40FFFF);
        outline = hexa(0x20595C);
    } else if (state == "InsufficientResources") {
        font = hexa(0xFF5555);
        outline = hexa(0x501717);
    } else {
        return false;
    }
    return true;
}

static Color rgba(const Json& v, const Color& fallback) {
    if (v.size() < 3) return fallback;
    return Color(v[(size_t)0].f(), v[1].f(), v[2].f(), v.size() > 3 ? v[3].f() : 1.0f);
}

Node* make_card(SceneLibrary* lib, const Json& c) {
    std::string err;
    Node* card = lib->instantiate("scenes/cards/card.tscn", &err);
    if (!card) {
        log_warn("card: %s", err.c_str());
        return NULL;
    }
    const std::string type = c["type"].str();
    std::string t = lower(type);
    if (t != "attack" && t != "skill" && t != "power" && t != "quest") t = "skill";
    std::string border = t == "quest" ? std::string("skill") : t;
    std::string banner;
    Color title_outline;
    rarity_style(c["rarity"].str(), banner, title_outline);
    const std::string banner_mat = "materials/cards/banners/card_banner_" + banner + "_mat.tres";

    /* Not an Ancient card, not locked: the plain frame. */
    static const char* const hide[] = { "AncientPortrait", "AncientBorderGlassOverlay", "AncientBorder", "AncientTextBg",
                                        "AncientBanner", "Lock", "EnchantmentVfxOverride", "CardSparkles" };
    for (size_t i = 0; i < sizeof(hide) / sizeof(hide[0]); ++i) {
        CanvasItem* n = item(card, hide[i]);
        if (n) n->visible = false;
    }

    CanvasItem* frame = item(card, "Frame");
    if (frame) {
        frame->visible = true;
        set_texture(lib, frame, "images/atlases/ui_atlas.sprites/card/card_frame_" + t + "_s.tres");
        if (c["frame_material"].is_string())
            set_material(lib, frame, "materials/cards/frames/" + c["frame_material"].str() + "_mat.tres");
    }
    CanvasItem* pborder = item(card, "PortraitBorder");
    if (pborder) {
        pborder->visible = true;
        set_texture(lib, pborder, "images/atlases/ui_atlas.sprites/card/card_portrait_border_" + border + "_s.tres");
        set_material(lib, pborder, banner_mat);
    }
    set_material(lib, item(card, "TitleBanner"), banner_mat);
    set_material(lib, item(card, "TypePlaque"), banner_mat);
    if (c["portrait"].is_string()) set_texture(lib, item(card, "Portrait"), c["portrait"].str());

    /* Title: cream with the rarity's outline; green once upgraded. */
    CanvasItem* title = item(card, "TitleLabel");
    set_text(title, c["title"].is_string() ? c["title"].str() : c["name"].str());
    if (c["upgrade_level"].i() > 0) colors(title, hexa(0x7FFF00), hexa(0x1B6131));
    else colors(title, CREAM, title_outline);

    /* Description: the game's own formatted text. */
    set_text(item(card, "DescriptionLabel"), "[center]" + c["description_bbcode"].str() + "[/center]");

    /* Type plaque: CARD_TYPE.<TYPE>, at least 61 wide, label + 17. */
    CanvasItem* tl = item(card, "TypeLabel");
    CanvasItem* plaque = item(card, "TypePlaque");
    set_text(tl, Loc::get("gameplay_ui", ("CARD_TYPE." + upper(type.empty() ? std::string("None") : type)).c_str()));
    if (tl && plaque && tl->type == "Label" && plaque->is_control()) {
        float w = ((Label*)tl)->get_minimum_size().x + 17.0f;
        if (w < 61.0f) w = 61.0f;
        Control* p = (Control*)plaque;
        float mid = (p->offset[SIDE_LEFT] + p->offset[SIDE_RIGHT]) * 0.5f;
        p->offset[SIDE_LEFT] = mid - w * 0.5f;
        p->offset[SIDE_RIGHT] = mid + w * 0.5f;
    }

    /* Energy cost orb: hidden for unplayable cards (cost -1). */
    CanvasItem* energy = item(card, "EnergyIcon");
    const std::string cost = c["cost_display"].is_string() ? c["cost_display"].str() : format("%d", c["cost"].i());
    if (energy) {
        energy->visible = cost == "X" || (!cost.empty() && cost[0] != '-');
        if (c["energy_icon"].is_string())
            set_texture(lib, energy, "images/atlases/ui_atlas.sprites/card/energy_" + c["energy_icon"].str() + ".tres");
        CanvasItem* label = item(card, "EnergyLabel");
        set_text(label, cost);
        Color font = CREAM, outline = rgba(c["energy_outline"], hexa(0x5C5440));
        cost_color(c["cost_color"].str(), c["cost_just_upgraded"].b(), font, outline);
        colors(label, font, outline);
        CanvasItem* unplayable = item(card, "UnplayableEnergyIcon");
        if (unplayable) unplayable->visible = c["unplayable_icon"].b();
    }
    CanvasItem* star = item(card, "StarIcon");
    if (star) {
        const std::string sc = c["star_display"].str();
        star->visible = sc == "X" || (!sc.empty() && sc[0] != '-');
        CanvasItem* label = item(card, "StarLabel");
        set_text(label, sc);
        Color font = CREAM, outline = hexa(0x175561, 0xDC / 255.0f);
        cost_color(c["star_color"].str(), c["star_just_upgraded"].b(), font, outline);
        colors(label, font, outline);
        CanvasItem* unplayable = item(card, "UnplayableStarIcon");
        if (unplayable) unplayable->visible = c["unplayable_icon"].b();
    }
    /* NCard.UpdateEnchantmentVisuals: the tab with the enchantment's icon
     * (its art, else the beta art) and amount; dimmed while disabled (hsv
     * s 0.1 instead of 0.4, alpha 0.9, the icon through the tab's material,
     * the amount gray); 45 px higher on cards without a star cost. */
    CanvasItem* ench = item(card, "Enchantment");
    const Json& e = c["enchantment_view"];
    if (ench) ench->visible = e.is_object();
    if (ench && e.is_object()) {
        Resource* icon_res = lib->ext(e["icon"].str());
        std::string icon = icon_res ? e["icon"].str() : e["beta_icon"].str();
        Node* icon_node = ench->get_node("Icon");
        if (lib->ext(icon) && icon_node && icon_node->is_canvas_item()) set_texture(lib, (CanvasItem*)icon_node, icon);
        Node* amount = ench->get_node("Label");
        set_text(amount && amount->is_canvas_item() ? (CanvasItem*)amount : NULL, format("%d", e["amount"].i()));
        if (amount && amount->is_canvas_item()) ((CanvasItem*)amount)->visible = e["show_amount"].b();
        bool disabled = e["disabled"].b();
        ench->modulate = Color(1, 1, 1, disabled ? 0.9f : 1.0f);
        ench->shader_params["h"] = 0.25f;
        ench->shader_params["s"] = disabled ? 0.1f : 0.4f;
        ench->shader_params["v"] = 0.6f;
        if (icon_node && icon_node->is_canvas_item()) ((CanvasItem*)icon_node)->use_parent_material = disabled;
        if (amount && amount->is_canvas_item()) ((CanvasItem*)amount)->self_modulate = disabled ? Color(0.5f, 0.5f, 0.5f, 1) : Color(1, 1, 1, 1);
        const std::string sc = c["star_display"].str();
        bool star_cost = sc == "X" || (!sc.empty() && sc[0] != '-');
        if (!star_cost && ench->is_control()) {
            Control* tab = (Control*)ench;
            tab->offset[SIDE_TOP] -= 45;
            tab->offset[SIDE_BOTTOM] -= 45;
        }
    }

    /* NCard.ReloadOverlay: the affliction's overlay, else the card's own. */
    if (c["overlay"].is_string()) {
        Node* holder = card->get_node("CardContainer/OverlayContainer");
        std::string oerr;
        Node* overlay = lib->ext(c["overlay"].str()) ? lib->instantiate(c["overlay"].str(), &oerr) : NULL;
        if (holder && overlay) holder->add_child(overlay);
        else delete overlay;
    }

    /* Ancient cards (NCard.UpdateVisuals / UpdatePortrait): the full-art
     * portrait through the canvas group's rounded mask, the ancient border,
     * text background and banner (its fire), no frame, portrait border or
     * title banner. */
    if (c["rarity"].str() == "Ancient") {
        static const char* const off[] = { "Portrait", "PortraitBorder", "Frame", "TitleBanner" };
        for (size_t i = 0; i < sizeof(off) / sizeof(off[0]); ++i) {
            CanvasItem* n = item(card, off[i]);
            if (n) n->visible = false;
        }
        static const char* const on[] = { "AncientPortrait", "AncientBorderGlassOverlay", "AncientBorder", "AncientTextBg",
                                          "AncientBanner" };
        for (size_t i = 0; i < sizeof(on) / sizeof(on[0]); ++i) {
            CanvasItem* n = item(card, on[i]);
            if (n) n->visible = true;
        }
        if (c["portrait"].is_string()) set_texture(lib, item(card, "AncientPortrait"), c["portrait"].str());
        if (c["ancient_border"].is_string()) set_texture(lib, item(card, "AncientBorder"), c["ancient_border"].str());
        if (c["ancient_text_bg"].is_string()) set_texture(lib, item(card, "AncientTextBg"), c["ancient_text_bg"].str());
        CanvasItem* group = item(card, "PortraitCanvasGroup");
        if (group && group->type == "CanvasGroup") {
            set_material(lib, group, "scenes/cards/card_canvas_group_mask_material.tres");
            ((CanvasGroup*)group)->refresh();
        }
    }

    /* NCard.Visibility (the card library): an unseen or locked card keeps
     * its frame, its title and description say so, its portrait blurred
     * (card_portrait_blur_material), the lock on a locked one. */
    const std::string& vis = c["visibility"].str();
    if (vis == "not_seen" || vis == "locked") {
        bool locked = vis == "locked";
        set_text(title, Loc::get("card_library", locked ? "LOCKED.title" : "UNKNOWN.title"));
        colors(title, CREAM, title_outline);
        set_text(item(card, "DescriptionLabel"), "[center][font_size=40]" +
                                                     Loc::get("card_library", locked ? "LOCKED.description" : "UNKNOWN.description") +
                                                     "[/font_size][/center]");
        CanvasItem* lock = item(card, "Lock");
        if (lock) lock->visible = locked;
        set_material(lib, item(card, "Portrait"), "scenes/cards/card_portrait_blur_material.tres");
        set_material(lib, item(card, "AncientPortrait"), "scenes/cards/card_portrait_blur_material.tres");
    }

    /* The hand's glow (NCardHighlight): playable cyan, gold, red. */
    CanvasItem* glow = item(card, "Highlight");
    if (glow) {
        const std::string g = c["glow"].str();
        glow->visible = g == "playable" || g == "gold" || g == "red";
        glow->shader_params["width"] = glow->visible ? 0.075f : 0.0f; /* AnimShow's end value */
        if (g == "gold") glow->modulate = Color(1, 0.784f, 0, 0.98f);
        else if (g == "red") glow->modulate = Color(0.83f, 0, 0.33f, 0.98f);
        else glow->modulate = Color(0, 0.957f, 0.988f, 0.98f);
    }
    return card;
}

void set_card_highlight(Node* card, bool on) {
    if (!card) return;
    CanvasItem* glow = item(card, "Highlight");
    if (!glow) return;
    glow->visible = on;
    glow->shader_params["width"] = on ? 0.075f : 0.0f;
    /* NCardHighlight.playableColor, gold for a rare reward card */
    glow->modulate = find_named(card, "RareGlow") ? Color(1, 0.784f, 0, 0.98f) : Color(0, 0.957f, 0.988f, 0.98f);
}

namespace {
/* NCardRareGlow._Ready: after a random 0..0.2 s, to 0.92 of its scale and
 * alpha 0.9 over 1 s (Cubic In); NCardUncommonGlow: its alpha only, at
 * once. */
class RewardGlow : public ScriptBehavior {
public:
    RewardGlow(Node2D* n, bool rare)
        : n_(n), rare_(rare), from_(n->scale), t_(rare ? -(double)(chaotic_rand() % 201) / 1000.0 : 0) {}
    virtual void process(Node*, double dt) {
        if (t_ > 1.0) return;
        t_ += dt;
        if (t_ < 0) return;
        float k = tween_interpolate(TRANS_CUBIC, EASE_IN, clampf((float)t_, 0, 1), 0, 1, 1);
        if (rare_) n_->scale = from_ + (from_ * 0.92f - from_) * k;
        n_->modulate.a = 1.0f - 0.1f * k;
    }

private:
    Node2D* n_;
    bool rare_;
    Vec2 from_;
    double t_;
};
} /* namespace */

void card_reward_glow(SceneLibrary* lib, Node* card, const std::string& rarity) {
    bool rare = rarity == "Rare";
    if (!card || (!rare && rarity != "Uncommon")) return;
    Node* body = card->find_unique("CardContainer");
    if (!body) return;
    if (rare) set_visible(body->get_node("CardSparkles"), true);
    Node* fx = instance(lib, rare ? "scenes/vfx/rare_glow_vfx.tscn" : "scenes/vfx/uncommon_glow_vfx.tscn");
    if (!fx || !fx->is_node2d()) {
        delete fx;
        return;
    }
    fx->behavior = new RewardGlow((Node2D*)fx, rare);
    /* Body.MoveChildSafely(glow, 1) */
    if (body->children.size() > 1) body->add_child_before(fx, body->children[1]);
    else body->add_child(fx);
}

} /* namespace spire */
