#include "card_view.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/text_nodes.h"
#include "loc.h"

#include <ctype.h>

namespace spire {

static std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

static std::string upper(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = (char)toupper((unsigned char)s[i]);
    return s;
}

static Color hex(unsigned v, float a = 1.0f) {
    return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, a);
}

static const Color CREAM(1, 0.964706f, 0.886275f);

static CanvasItem* item(Node* scope, const char* unique) {
    Node* n = scope->find_unique(unique);
    if (!n) n = scope->get_node(std::string("CardContainer/") + unique);
    return n && n->is_canvas_item() ? (CanvasItem*)n : NULL;
}

static void set_texture(SceneLibrary* lib, CanvasItem* n, const std::string& path) {
    if (!n) return;
    Resource* r = lib->ext(path);
    Texture2DRes* t = r && r->is_texture() ? (Texture2DRes*)r : NULL;
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
    outline = hex(0x4D4B40);
    if (rarity == "Uncommon") {
        banner = "uncommon";
        outline = hex(0x005C75);
    } else if (rarity == "Rare") {
        banner = "rare";
        outline = hex(0x6B4B00);
    } else if (rarity == "Curse") {
        banner = "curse";
        outline = hex(0x550B9E);
    } else if (rarity == "Status") {
        banner = "status";
        outline = hex(0x4F522F);
    } else if (rarity == "Event") {
        banner = "event";
        outline = hex(0x1B6131);
    } else if (rarity == "Quest") {
        banner = "quest";
        outline = hex(0x7E3E15);
    } else if (rarity == "Ancient") {
        banner = "ancient";
    }
}

/* CardCostHelper colours: Increased, Decreased, InsufficientResources. */
static bool cost_color(const std::string& state, bool just_upgraded, Color& font, Color& outline) {
    if (just_upgraded || state == "Decreased") {
        font = hex(0x7FFF00);
        outline = hex(0x1F5923);
    } else if (state == "Increased") {
        font = hex(0x40FFFF);
        outline = hex(0x20595C);
    } else if (state == "InsufficientResources") {
        font = hex(0xFF5555);
        outline = hex(0x501717);
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
    if (c["upgrade_level"].i() > 0) colors(title, hex(0x7FFF00), hex(0x1B6131));
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
        Color font = CREAM, outline = rgba(c["energy_outline"], hex(0x5C5440));
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
        Color font = CREAM, outline = hex(0x175561, 0xDC / 255.0f);
        cost_color(c["star_color"].str(), c["star_just_upgraded"].b(), font, outline);
        colors(label, font, outline);
        CanvasItem* unplayable = item(card, "UnplayableStarIcon");
        if (unplayable) unplayable->visible = c["unplayable_icon"].b();
    }
    CanvasItem* ench = item(card, "Enchantment");
    if (ench) ench->visible = false; /* enchantment tabs come later */

    /* The hand's glow (NCardHighlight): playable cyan, gold, red. */
    CanvasItem* glow = item(card, "Highlight");
    if (glow) {
        const std::string g = c["glow"].str();
        glow->visible = g == "playable" || g == "gold" || g == "red";
        if (g == "gold") glow->modulate = Color(1, 0.784f, 0, 0.98f);
        else if (g == "red") glow->modulate = Color(0.83f, 0, 0.33f, 0.98f);
        else glow->modulate = Color(0, 0.957f, 0.988f, 0.98f);
    }
    return card;
}

} /* namespace spire */
