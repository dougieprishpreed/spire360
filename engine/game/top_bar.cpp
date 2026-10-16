#include "top_bar.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

static void set_texture(SceneLibrary* lib, Node* rect, const std::string& path) {
    if (!rect || rect->type != "TextureRect") return;
    Resource* r = path.empty() ? NULL : lib->ext(path);
    ((TextureRect*)rect)->texture = r && r->is_texture() ? (Texture2DRes*)r : NULL;
}

static void hide(Node* n) {
    if (n && n->is_canvas_item()) ((CanvasItem*)n)->visible = false;
}

/* ImageHelper.GetRoomIconPath: images/ui/run_history/<suffix>.png, "" for
 * none (the act-start map room). */
static std::string room_icon(const Json& ctx) {
    const std::string& point = ctx["point_type"].str();
    const std::string& room = ctx["room_type"].str();
    if (point.empty() || point == "Unassigned" || room.empty() || room == "Map") return std::string();
    std::string suffix;
    if (point == "Boss") {
        const Json& second = ctx["second_boss"];
        suffix = ctx["at_second_boss"].b() && second.is_string() ? second.str() : ctx["boss"]["id"].str();
    } else if (point == "Ancient") {
        suffix = ctx["ancient"].str();
    } else if (room != "Boss") {
        /* Slugify(roomType): RestSite -> rest_site */
        std::string slug;
        for (size_t i = 0; i < room.size(); ++i) {
            if (i > 0 && room[i] >= 'A' && room[i] <= 'Z') slug += '_';
            slug += room[i];
        }
        suffix = point == "Unknown" && room != "Event" ? "unknown_" + slug : slug;
    }
    if (suffix.empty()) return std::string();
    return "images/ui/run_history/" + lower(suffix) + ".png";
}

/* An icon and its Outline child from a run_history path. */
static void set_room_icon(SceneLibrary* lib, Node* icon, const std::string& path) {
    if (!icon) return;
    if (path.empty()) {
        hide(icon);
        return;
    }
    set_texture(lib, icon, path);
    set_texture(lib, icon->get_node("Outline"), path.substr(0, path.size() - 4) + "_outline.png");
}

/* NTopBarButton glyphs: Map = Back, Deck = LB, Pause = Start; a disabled
 * button is greyed (StsColors.disabledTopBarButton) without its glyph. */
static void top_button(SceneLibrary* lib, Node* button, const char* glyph, bool enabled) {
    if (!button) return;
    Node* hk = button->find_unique("HotkeyIcon");
    if (enabled) show_glyph(lib, hk, glyph);
    else {
        hide(hk);
        if (button->is_canvas_item()) ((CanvasItem*)button)->modulate = Color(0.5f, 0.5f, 0.5f, 0.75f);
    }
}

/* NPotionContainer: one holder per slot (player.max_potions), a potion
 * (potion.tscn, 0.9 scale, centred) in the slots that have one. */
static void make_potions(SceneLibrary* lib, Node* bar, const Json& player) {
    Node* holders = bar->find_unique("PotionHolders");
    if (!holders) holders = find_named(bar, "PotionHolders");
    if (!holders) return;
    const Json& potions = player["potions"];
    int slots = player["max_potions"].i(3);
    for (size_t i = 0; i < potions.size(); ++i)
        if (potions[i]["index"].i() + 1 > slots) slots = potions[i]["index"].i() + 1;
    for (int s = 0; s < slots; ++s) {
        Node* h = instance(lib, "scenes/potions/potion_holder.tscn");
        if (!h || !h->is_control()) {
            delete h;
            return;
        }
        const Json* pot = NULL;
        for (size_t i = 0; i < potions.size(); ++i)
            if (potions[i]["index"].i() == s) pot = &potions[i];
        Node* empty = h->find_unique("EmptyIcon");
        if (empty && empty->is_canvas_item()) {
            ((CanvasItem*)empty)->modulate.a = pot ? 0.0f : 1.0f;
            if (empty->is_control()) ((Control*)empty)->scale = Vec2(0.9f, 0.9f);
        }
        hide(h->find_unique("SelectionReticle"));
        if (pot) {
            Node* p = instance(lib, "scenes/potions/potion.tscn");
            if (p && p->is_control()) {
                Control* pc = (Control*)p;
                /* Position (-30, -30) from the holder's centre. */
                pc->anchor[SIDE_LEFT] = pc->anchor[SIDE_RIGHT] = 0.5f;
                pc->anchor[SIDE_TOP] = pc->anchor[SIDE_BOTTOM] = 0.5f;
                place(pc, -30, -30, 60, 60);
                pc->pivot_offset = Vec2(30, 30);
                pc->scale = Vec2(0.9f, 0.9f);
                std::string id = lower((*pot)["id"].str());
                Node* image = p->find_unique("Image");
                set_texture(lib, image, "images/atlases/potion_atlas.sprites/" + id + ".tres");
                if (image) set_texture(lib, image->find_unique("Outline"), "images/atlases/potion_outline_atlas.sprites/" + id + ".tres");
                hide(p->find_unique("Juice"));
                hide(p->find_unique("Overlay"));
                hide(p->find_unique("Bottle"));
                h->add_child(p);
            } else {
                delete p;
            }
        }
        holders->add_child(h);
    }
}

/* NRelicInventory: the relics in acquisition order, 68 px each, in a row
 * under the bar (run.tscn, which places it, is not in the bundles: the
 * game's screenshots show the row from x ~11, y ~81). */
static Node* make_relics(SceneLibrary* lib, const Json& player) {
    FlowContainer* row = (FlowContainer*)SceneLibrary::create_node("HFlowContainer");
    row->name = "RelicInventory";
    place(row, 11, 81, 1898, 68);
    row->mouse_filter = 2;
    const Json& relics = player["relics"];
    for (size_t i = 0; i < relics.size(); ++i) {
        Node* r = instance(lib, "scenes/relics/relic.tscn");
        if (!r || !r->is_control()) {
            delete r;
            break;
        }
        ((Control*)r)->custom_minimum_size = Vec2(68, 68);
        std::string id = lower(relics[i]["id"].str());
        Node* icon = r->find_unique("Icon");
        set_texture(lib, icon, "images/atlases/relic_atlas.sprites/" + id + ".tres");
        if (icon) set_texture(lib, icon->find_unique("Outline"), "images/atlases/relic_outline_atlas.sprites/" + id + ".tres");
        row->add_child(r);
    }
    return row;
}

Node* make_top_bar(SceneLibrary* lib, const Json& d, bool in_map_room) {
    Node* bar = instance(lib, "scenes/ui/top_bar.tscn");
    if (!bar) return NULL;
    const Json& player = d["player"];
    const Json& ctx = d["context"];
    std::string character = lower(player["character"].is_string() ? player["character"].str() : std::string("ironclad"));

    /* Portrait (Character.Icon) and ascension. */
    Node* portrait = bar->find_unique("TopBarPortrait");
    if (!portrait) portrait = find_named(bar, "TopBarPortrait");
    Node* icon = instance(lib, "scenes/ui/character_icons/" + character + "_icon.tscn");
    if (portrait && icon) portrait->add_child(icon);
    else delete icon;
    int asc = player["ascension"].i();
    Node* asc_icon = bar->find_unique("AscensionIcon");
    if (asc_icon && asc_icon->is_canvas_item()) {
        ((CanvasItem*)asc_icon)->visible = asc > 0;
        Node* label = asc_icon->get_node("AscensionLabel");
        if (label && label->is_control()) {
            ((Control*)label)->add_theme_color_override("font_outline_color", hex(0x593400));
            set_label(label, format("%d", asc));
        }
    }
    hide(bar->find_unique("AchievementLock"));

    /* HP and gold. */
    set_label(bar->find_unique("HpLabel"), format("%d/%d", player["hp"].i(), player["max_hp"].i()));
    set_label(bar->find_unique("GoldLabel"), format("%d", player["gold"].i()));

    make_potions(lib, bar, player);
    Node* shortcut = find_named(bar, "PotionShortcutButton");
    show_glyph(lib, shortcut ? shortcut->find_unique("HotkeyIcon") : NULL, "x");

    /* Room, floor, boss. */
    Node* room_icon_node = find_named(bar, "RoomIcon");
    set_room_icon(lib, room_icon_node ? room_icon_node->get_node("Icon") : NULL, room_icon(ctx));
    set_label(find_named(bar, "FloorNumLabel"), format("%d", ctx["total_floor"].i(ctx["floor"].i())));
    Node* boss = find_named(bar, "BossIcon");
    if (boss) {
        bool in_boss_room = ctx["room_type"].str() == "Boss";
        if (in_boss_room) hide(boss);
        else if (ctx["boss"]["id"].is_string())
            set_room_icon(lib, boss->get_node("Icon"), "images/ui/run_history/" + lower(ctx["boss"]["id"].str()) + ".png");
    }
    hide(find_named(bar, "Modifiers"));

    /* Right side: the save indicator is transparent at rest, the timer
     * hidden (shown only with the map or a capstone screen open). */
    Node* save = find_named(bar, "SaveIndicator");
    if (save && save->is_canvas_item()) ((CanvasItem*)save)->modulate.a = 0;
    hide(find_named(bar, "TimerContainer"));
    top_button(lib, find_named(bar, "Map"), "back", !in_map_room);
    top_button(lib, find_named(bar, "Deck"), "lb", true);
    top_button(lib, find_named(bar, "PauseButton"), "start", true);
    Node* deck = find_named(bar, "Deck");
    set_label(find_named(deck, "DeckCardCount"), format("%d", player["deck_size"].i()));
    hide(find_named(bar, "GoldPopup"));

    Control* root = full_rect_control("TopBarLayer");
    root->add_child(make_relics(lib, player));
    root->add_child(bar);
    return root;
}

} /* namespace spire */
