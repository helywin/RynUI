#include "component/tree_component.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "support/timeline_fixture.hpp"
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.scene.glyph_scene().instances().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "Tree cleanup leaked resources, blocks or deadlines");
}

void project_tree() {
    ryn::Tree(ryn::TreeProps{}
                  .checkable()
                  .defaultExpandedKeys({u8"project"})
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(320)).height(ryn::dp(200))),
              {ryn::TreeItem{
                  u8"project", u8"Project", {ryn::TreeItem{u8"src", u8"Sources"}, ryn::TreeItem{u8"docs", u8"Docs"}}}});
}

void basic() {
    Fixture f;
    f.services.mount(Content{project_tree});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    auto s = f.services.tree().snapshot(id);
    check(s.rows.size() == 3 && s.rows[1].shown && s.rows[0].expanded, "documented Tree example did not expand");
    click(f, s.rows[0].checkbox);
    s = f.services.tree().snapshot(id);
    check(s.checked == TreeKeys{u8"project", u8"src", u8"docs"} && s.half_checked.empty(),
          "pointer checkbox did not conduct");
    click(f, s.rows[1].checkbox);
    s = f.services.tree().snapshot(id);
    check(s.checked == TreeKeys{u8"docs"} && s.half_checked == TreeKeys{u8"project"}, "half-check state missing");
    click(f, s.rows[1].title);
    check(f.services.tree().snapshot(id).selected == TreeKeys{u8"src"}, "pointer title did not select");
    key(f, Key::home);
    check(f.services.focus().state().focused == s.rows[0].interaction, "Home did not focus first row");
    key(f, Key::left);
    check(!f.services.tree().snapshot(id).rows[1].shown, "Left did not collapse");
    key(f, Key::right);
    key(f, Key::right);
    check(f.services.focus().state().focused == s.rows[1].interaction, "Right did not enter child");
    key(f, Key::down);
    key(f, Key::enter);
    check(f.services.tree().snapshot(id).selected == TreeKeys{u8"docs"}, "keyboard selection failed");
    key(f, Key::space);
    check(f.services.tree().snapshot(id).checked.empty(), "Space did not uncheck remaining leaf");
    clean(f);
}

void controlled() {
    Fixture f;
    Signal<TreeKeys> expanded{TreeKeys{}};
    Signal<TreeKeys> selected{TreeKeys{u8"unknown", u8"a", u8"b"}};
    Signal<TreeKeys> checked{TreeKeys{}};
    Signal<bool> checkable{true};
    TreeExpandInfo expansion;
    TreeSelectInfo selection;
    TreeCheckInfo checking;
    bool keyboard_modality = false;
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .expandedKeys(expanded)
                 .selectedKeys(selected)
                 .checkedKeys(checked)
                 .checkable(checkable)
                 .onExpand([&](auto v) { expansion = v; })
                 .onSelect([&](auto v) {
                     selection = v;
                     keyboard_modality = f.services.focus().state().focus_visible;
                 })
                 .onCheck([&](auto v) { checking = v; }),
             {TreeItem{u8"root", u8"Root", {TreeItem{u8"a", u8"A"}, TreeItem{u8"b", u8"B"}}}});
    }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    auto s = f.services.tree().snapshot(id);
    check(s.selected == TreeKeys{u8"a"}, "single controlled selection not normalized");
    click(f, s.rows[0].switcher);
    check(expansion.expanded && expansion.expanded_keys == TreeKeys{u8"root"} &&
              !f.services.tree().snapshot(id).rows[0].expanded,
          "controlled expansion mutated before acceptance");
    expanded.set(expansion.expanded_keys);
    sync(f);
    click(f, s.rows[2].checkbox);
    check(checking.checked_keys == TreeKeys{u8"b"} && checking.half_checked_keys == TreeKeys{u8"root"} &&
              f.services.tree().snapshot(id).checked.empty(),
          "controlled check mutated before acceptance");
    checked.set(checking.checked_keys);
    click(f, s.rows[2].title);
    check(selection.selected_keys == TreeKeys{u8"b"} && f.services.tree().snapshot(id).selected == TreeKeys{u8"a"},
          "controlled selection mutated before acceptance");
    selected.set(selection.selected_keys);
    sync(f);
    // Start in pointer modality and consume Enter in the Tree command filter.
    click(f, s.rows[0].title);
    key(f, Key::enter);
    check(keyboard_modality, "consumed Tree keyboard command did not establish keyboard modality before callback");
    checkable.set(false);
    sync(f);
    key(f, Key::space);
    check(selection.key == u8"root", "noncheckable Space did not select");
    clean(f);
}

void rich() {
    Fixture f;
    InputRef ref;
    Signal<bool> visible{true};
    Signal<bool> disabled{false};
    Signal<bool> checkable{false};
    Signal<TreeKeys> expanded{TreeKeys{u8"root"}};
    Signal<String> title{String{u8"Root"}};
    int mounts = 0;
    int clicked = 0;
    int selections = 0;
    f.services.mount(Content{[&] {
        Button(ButtonProps{}.wave(false), [] { Text(u8"Before"); });
        Tree(TreeProps{}
                 .expandedKeys(expanded)
                 .checkable(checkable)
                 .onSelect([&](auto) { ++selections; })
                 .layout(LayoutStyle{}.width(dp(320)).height(dp(300))),
             {TreeItem{u8"root",
                       title,
                       {TreeItem{u8"child", u8"Rich"}.visible(visible).disabled(disabled).richTitle(TreeTitle{[&] {
                           ++mounts;
                           Flex(FlexProps{}.vertical(true).gap(dp(2)), FlexContent{[&] {
                                    Input(InputProps{}.ref(ref).defaultValue(u8"Original"));
                                    Button(ButtonProps{}.onClick([&] { ++clicked; }).wave(false),
                                           [] { Text(u8"Action"); });
                                    Select(SelectProps{}.options({{u8"a", u8"Alpha"}, {u8"b", u8"Beta"}}));
                                }});
                       }})}}});
        Button(ButtonProps{}.wave(false), [] { Text(u8"After"); });
    }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    const auto first = f.services.tree().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto button = f.buttons.mounted_buttons()[1];
    const auto count = f.services.components().component_count();
    check(ref.focus({InputFocusCursor::All}), "rich Input ref focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Retained draft"}, f.inputs.sessions().active()})),
          "rich Input draft commit failed");
    key(f, Key::home);
    key(f, Key::down);
    check(f.services.focus().state().focused == input.interaction && selections == 0, "Tree consumed rich Input key");
    title.set(u8"Updated");
    checkable.set(true);
    sync(f);
    check(f.services.components().component_count() == count && mounts == 1,
          "presentation update remounted rich slots");
    disabled.set(true);
    sync(f);
    click(f, button.component);
    check(clicked == 1 && ref.focus(), "disabled row wrapper sealed an independently enabled rich child");
    title.set(u8"Disabled row still hosts its own editor");
    checkable.set(false);
    sync(f);
    check(f.services.focus().state().focused == input.interaction,
          "unrelated Tree refresh stole focus from a valid rich child in a disabled row");
    disabled.set(false);
    check(ref.focus(), "rich ref failed after disabled recovery");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"预编辑"}, {0, 1}, stamp})),
          "rich preedit fixture failed");
    visible.set(false);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid() &&
              !f.inputs.editors().require(input.editor).composition().active &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})),
          "hidden rich input retained session/preedit");
    visible.set(true);
    expanded.set(TreeKeys{});
    sync(f);
    check(!ref.focus() && !f.services.tree().snapshot(id).rows[1].shown, "collapsed rich child remained focusable");
    expanded.set(TreeKeys{u8"root"});
    sync(f);
    check(ref.focus() && mounts == 1 && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Retained draft" &&
              f.services.tree().snapshot(id).rows[1].title == first.rows[1].title,
          "rich identity/ref/draft lost on restore");
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "rich Select did not open");
    const auto foreign = f.buttons.mounted_buttons().back().interaction;
    check(f.services.focus().request_focus(foreign, FocusModality::pointer), "foreign focus failed");
    expanded.set(TreeKeys{});
    sync(f);
    check(!f.services.select().snapshot(select).visible, "unfocused Select popup survived collapse");
    expanded.set(TreeKeys{u8"root"});
    sync(f);
    check(!f.services.select().snapshot(select).visible, "restoring collapsed branch reopened stale Select popup");
    // Declaration order remains stable when checkbox visibility or expansion changes.
    f.services.focus().clear_focus();
    key(f, Key::tab);
    check(f.services.focus().state().focused == f.buttons.mounted_buttons()[0].interaction, "Before lost Tab position");
    key(f, Key::tab);
    check(f.services.focus().state().focused == first.rows[0].interaction, "Tree lost single Tab entry");
    expanded.set(TreeKeys{});
    sync(f);
    key(f, Key::tab);
    check(f.services.focus().state().focused == foreign, "collapsed Tree left hidden rich children in Tab order");
    clean(f);
}

void scroll() {
    Fixture f;
    std::vector<TreeItem> items;
    for (int i = 0; i < 24; ++i) {
        items.emplace_back(String::from_utf8(std::to_string(i)).value(), String{u8"Long scroll row"});
    }
    f.services.mount(Content{[&] { Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(180)).height(dp(60))), items); }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    const auto s = f.services.tree().snapshot(id);
    check(s.content_height > 60, "finite Tree did not retain scrollable content");
    check(f.services.focus().request_focus(s.rows[0].interaction, FocusModality::pointer), "scroll row focus failed");
    key(f, Key::end);
    auto last = f.services.tree().snapshot(id);
    check(last.scroll > 0 && inside(bounds(f, last.rows.back().component), bounds(f, id)) &&
              f.services.focus().state().focused == last.rows.back().interaction,
          "End did not scroll focused row into finite viewport");
    key(f, Key::home);
    check(f.services.tree().snapshot(id).scroll == 0, "Home failed to reveal first row");
    check(f.services.dispatch_scroll({0, -2, 80, 26}), "Tree did not own wheel in its row gap");
    sync(f);
    check(f.services.tree().snapshot(id).scroll > 0, "wheel did not move actual rows");
    clean(f);
}

std::size_t ink(Fixture& f, runtime::ComponentId owner, runtime::Rect clip) {
    std::size_t count = 0;
    const auto& atlas = f.scene.atlas();
    const auto aw = atlas.config().page_width;
    const auto ah = atlas.config().page_height;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, owner)) {
            continue;
        }
        for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
            const auto bytes = atlas.page_bytes(range.atlas_page);
            for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(i);
                if (g.translation_opacity[2] <= 0 || g.color[3] <= 0) {
                    continue;
                }
                check(inside({g.clip_bounds[0], g.clip_bounds[1], g.clip_bounds[2] - g.clip_bounds[0],
                              g.clip_bounds[3] - g.clip_bounds[1]},
                             clip),
                      "actual Tree glyph clip escaped its partition");
                const int x0 = static_cast<int>(std::lround(g.uv_rect[0] * aw));
                const int y0 = static_cast<int>(std::lround(g.uv_rect[1] * ah));
                const int x1 = static_cast<int>(std::lround(g.uv_rect[2] * aw));
                const int y1 = static_cast<int>(std::lround(g.uv_rect[3] * ah));
                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        if (bytes[static_cast<std::size_t>(y) * aw + x] < 32) {
                            continue;
                        }
                        const auto p =
                            graphics::glyph_vertex(g, {(x - x0 + .5F) / (x1 - x0), (y - y0 + .5F) / (y1 - y0)});
                        if (p.x >= g.clip_bounds[0] && p.x <= g.clip_bounds[2] && p.y >= g.clip_bounds[1] &&
                            p.y <= g.clip_bounds[3]) {
                            check(contains(clip, p), "R8 Tree glyph ink escaped its allocated clip");
                            ++count;
                        }
                    }
                }
            }
        }
    }
    return count;
}

void clips() {
    Fixture f;
    Signal<LogicalLength> width{dp(200)};
    Signal<LogicalLength> height{dp(70)};
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .checkable()
                 .defaultExpandedKeys({u8"root"})
                 .defaultSelectedKeys({u8"child"})
                 .layout(LayoutStyle{}.width(width).height(height)),
             {TreeItem{u8"root", u8"Root", {TreeItem{u8"child", u8"Child"}.richTitle(TreeTitle{[] {
                           Text(TypographyProps{}.content(u8"汉汉 Long marked title").mark(true));
                           Button(ButtonProps{}.wave(false), [] { Text(u8"Child action"); });
                       }})}}});
    }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    const auto s = f.services.tree().snapshot(id);
    bool glyph = false;
    bool fill = false;
    bool mark = false;
    for (auto extent : {runtime::Size{200, 70}, runtime::Size{67, 25}, runtime::Size{5, 3}, runtime::Size{0, 0},
                        runtime::Size{320, 150}}) {
        width.set(dp(extent.width));
        height.set(dp(extent.height));
        sync(f);
        const auto allocated = bounds(f, id);
        const auto label_clip = graphics::intersect_effect_bounds(bounds(f, s.rows[1].title), allocated);
        glyph |= ink(f, s.rows[1].title, label_clip) > 0;
        for (const auto& hit : f.services.hit_test().records()) {
            const auto* record = f.services.interactions().find(hit.interaction);
            if (record && descendant(f, record->component, id)) {
                check(inside(hit.effective_clip, allocated), "actual Tree hit escaped finite viewport");
                if (descendant(f, record->component, s.rows[1].title)) {
                    check(inside(hit.effective_clip, label_clip), "rich child hit escaped title partition");
                }
            }
        }
        const auto t = f.services.components().theme_scope(id)->snapshot().tree();
        for (const auto& effect : f.services.rounded_effects().packed_instances()) {
            if (!effect.material.visible) {
                continue;
            }
            if (effect.material.color == t.colors.node_selected_background) {
                fill = true;
                const auto checkbox = bounds(f, s.rows[1].checkbox);
                check(effect.geometry.shape.rect.x >= checkbox.x + checkbox.width,
                      "selected Tree fill covered the checkbox partition");
            }
            check(effect.geometry.ancestor_clip.has_value(), "Tree scene effect has no clip");
            check(inside(effect.geometry.ancestor_clip->bounds, allocated), "actual Tree effect clip escaped viewport");
            check(graphics::rounded_effect_coverage({allocated.x - 3, allocated.y - 3}, effect) == 0,
                  "Tree rounded effect painted outside external clip");
        }
        const auto marker = f.services.components().theme_scope(id)->snapshot().typography().colors.mark_background;
        for (const auto& q : f.services.surfaces().instances().instances()) {
            if (q.opacity > 0 && q.color == std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()}) {
                mark = true;
                check(inside({q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                             label_clip),
                      "actual marked fill escaped title partition");
            }
        }
    }
    check(glyph && fill && mark, "clipping fixture emitted no actual glyph/fill/mark");
    check(f.services.focus().request_focus(s.rows[1].interaction, FocusModality::keyboard),
          "focus clip fixture failed");
    sync(f);
    bool outward = false;
    const auto row = bounds(f, s.rows[1].component);
    const auto t = f.services.components().theme_scope(id)->snapshot().tree();
    for (const auto& e : f.services.rounded_effects().packed_instances()) {
        if (e.material.visible && e.material.color == t.colors.focus &&
            e.geometry.kind == graphics::RoundedEffectKind::outline) {
            const auto shape = graphics::rounded_effect_bounds(e);
            const runtime::Point point{row.x + row.width / 2, row.y - 2};
            outward |= shape.y < row.y && e.geometry.ancestor_clip &&
                       contains(e.geometry.ancestor_clip->bounds, point) &&
                       graphics::rounded_effect_coverage(point, e) > 0;
        }
    }
    check(outward, "actual Tree focus decoration was cropped to row content bounds");
    clean(f);
}

void boundaries() {
    Fixture f;
    Signal<TreeKeys> expanded{TreeKeys{u8"root"}};
    Signal<bool> visible{true};
    Signal<bool> disabled{false};
    Signal<bool> open{true};
    int closes = 0;
    f.services.mount(Content{[&] {
        Tree(TreeProps{}.expandedKeys(expanded).visible(visible).disabled(disabled).layout(
                 LayoutStyle{}.width(dp(240)).height(dp(120))),
             {TreeItem{u8"root", u8"Root", {TreeItem{u8"child", u8"Popup"}.richTitle(TreeTitle{[&] {
                           Select(SelectProps{}.options({{u8"a", u8"Alpha"}}).open(open).onOpenChange([&](bool v) {
                               closes += !v;
                           }));
                       }})}}});
        Button(ButtonProps{}.wave(false), [] { Text(u8"Foreign"); });
    }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    const auto pop = f.services.select().mounted()[0];
    check(f.services.select().snapshot(pop).visible, "controlled popup fixture failed");
    // Window-layer scrolling must not fall through into its Tree ancestor.
    const auto before = f.services.tree().snapshot(id).scroll;
    const auto popup = f.services.select().snapshot(pop).popup;
    const auto box = popup;
    f.services.dispatch_scroll({0, -1, box.x + box.width / 2, box.y + box.height / 2});
    check(f.services.tree().snapshot(id).scroll == before, "child window layer wheel fell through Tree ownership");
    expanded.set(TreeKeys{});
    sync(f);
    check(!f.services.select().snapshot(pop).visible && closes > 0, "no-focus controlled popup survived collapse");
    expanded.set(TreeKeys{u8"root"});
    sync(f);
    check(!f.services.select().snapshot(pop).visible, "controlled stale open resurrected after collapse");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.select().snapshot(pop).visible, "explicit controlled reopen failed");
    disabled.set(true);
    sync(f);
    check(!f.services.select().snapshot(pop).visible, "global disable did not close child popup");
    disabled.set(false);
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.select().snapshot(pop).visible, "window inactive popup fixture failed");
    f.services.set_window_active(false);
    sync(f);
    check(!f.services.select().snapshot(pop).visible, "window inactive retained child popup");
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.select().snapshot(pop).visible, "window reactivation reopened stale popup");
    visible.set(false);
    sync(f);
    check(f.nodes.require(f.services.components().root(id)).out_of_flow, "hidden Tree remained in flow");
    visible.set(true);
    sync(f);
    check(!f.services.select().snapshot(pop).visible, "visibility restore reopened stale popup");
    const auto foreign = f.buttons.mounted_buttons().back().component;
    f.services.set_input_blocked(foreign, true);
    check(f.services.destroy(id), "Tree destroy failed");
    check(blocked_roots(f.services.interactions()) == std::vector{foreign}, "Tree cleanup removed foreign block");
    f.services.set_input_blocked(foreign, false);
    clean(f);
}

void mount_lifecycle() {
    for (int mode = 0; mode < 5; ++mode) {
        auto f = std::make_unique<Fixture>();
        f->services.mount(Content{[&] {
            TreeProps props;
            if (mode == 2) {
                props.visible(bind([&] {
                    const auto owner = f->services.tree().mounted()[0];
                    f->services.destroy(owner);
                    return true;
                }));
            }
            if (mode == 3) {
                props.layout(LayoutStyle{}.width(bind([&] {
                    const auto owner = f->services.tree().mounted()[0];
                    f->services.destroy(owner);
                    return dp(100);
                })));
            }
            auto item = TreeItem{u8"a", u8"A"}.richTitle(TreeTitle{[&] {
                if (mode == 0 || mode == 1) {
                    const auto owner = f->services.tree().mounted()[0];
                    const auto title = f->services.tree().snapshot(owner).rows[0].title;
                    f->services.destroy(mode == 0 ? owner : title);
                } else if (mode == 4) {
                    const auto owner = f->services.tree().mounted()[0];
                    const auto switcher = f->services.tree().snapshot(owner).rows[0].switcher;
                    f->services.destroy(switcher);
                }
            }});
            Tree(props, {item});
            Text(u8"Surviving sibling");
        }});
        sync(*f);
        check(f->services.tree().mounted().empty() && f->services.components().component_count() == 1 &&
                  blocked_roots(f->services.interactions()).empty(),
              "mount reentry left stale Tree/private resources");
        clean(*f);
    }
    // A signal changed during the first rich slot must be applied by the final mount refresh.
    Fixture f;
    Signal<bool> shown{true};
    f.services.mount(Content{[&] {
        Tree({}, {TreeItem{u8"a", u8"A"}.visible(shown).richTitle(TreeTitle{[&] {
                 shown.set(false);
                 Input(InputProps{}.defaultValue(u8"Retained"));
             }})});
    }});
    sync(f);
    check(!f.services.tree().snapshot(f.services.tree().mounted()[0]).rows[0].shown,
          "mount-time Signal update was skipped by first apply");
    clean(f);
}

void lifecycle() {
    Fixture f;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        parent = b.mount_component<int>(0);
        const auto node = b.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{{}, true});
        b.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    InputRef ref;
    Signal<bool> visible{true};
    runtime::ComponentId retired;
    bool replace = false;
    f.services.append_slot(
        parent, Content{[&] {
            Tree(TreeProps{}.visible(visible), {TreeItem{u8"a", u8"A"}.richTitle(TreeTitle{[&] {
                     Input(InputProps{}.ref(ref).onBlur([&] {
                         if (replace) {
                             replace = false;
                             f.services.destroy(retired);
                             f.services.append_slot(
                                 parent, Content{[&] {
                                     Tree({}, {TreeItem{u8"b", u8"B"}.richTitle(TreeTitle{[&] {
                                              Input(InputProps{}.ref(ref).defaultValue(u8"New generation"));
                                          }})});
                                 }});
                         }
                     }));
                 }})});
        }});
    sync(f);
    retired = f.services.tree().mounted()[0];
    check(ref.focus(), "lifecycle old input focus failed");
    const auto stamp = f.inputs.sessions().active();
    replace = true;
    visible.set(false);
    sync(f);
    check(f.services.tree().mounted().size() == 1 && f.services.tree().mounted()[0] != retired && ref.bound() &&
              ref.focus() && !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})) &&
              blocked_roots(f.services.interactions()).empty(),
          "old Tree generation touched replacement ref/input blocks");
    clean(f);
}

void validation() {
    for (int mode = 0; mode < 4; ++mode) {
        auto f = std::make_unique<Fixture>();
        bool rejected = false;
        try {
            f->services.mount(Content{[&] {
                if (mode == 0) {
                    Tree({}, {TreeItem{u8"a", u8"A"}, TreeItem{u8"a", u8"B"}});
                } else if (mode == 1) {
                    Tree(TreeProps{}.expandedKeys(TreeKeys{}).defaultExpandedKeys({}), {});
                } else if (mode == 2) {
                    std::vector<TreeItem> items;
                    for (int i = 0; i < 1025; ++i) {
                        items.emplace_back(String::from_utf8(std::to_string(i)).value(), String{u8"A"});
                    }
                    Tree({}, items);
                } else {
                    std::vector<TreeItem> items;
                    for (int i = 0; i < 65; ++i) {
                        items = {
                            TreeItem{String::from_utf8(std::to_string(i)).value(), String{u8"A"}, std::move(items)}};
                    }
                    Tree({}, items);
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f->services.tree().mounted().empty(), "invalid Tree accepted or partially mounted");
        clean(*f);
    }
}

void reactivity() {
    for (int mode = 0; mode < 3; ++mode) {
        auto f = std::make_unique<Fixture>();
        Signal<int> trigger{0};
        auto config = std::make_unique<Signal<ThemeConfig>>(ThemeConfig{});
        theme_runtime::Subscription subscription;
        f->services.mount(Content{[&] {
            Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                      Tree({}, {TreeItem{u8"a", u8"A"}
                                    .visible(bind([&] {
                                        const auto value = trigger.get();
                                        if (value && mode == 0) {
                                            const auto owner = f->services.tree().mounted()[0];
                                            f->services.destroy(f->services.tree().snapshot(owner).rows[0].title);
                                        }
                                        return true;
                                    }))
                                    .richTitle(TreeTitle{[&] {
                                        if (mode == 1) {
                                            const auto scope = runtime::require_component_build_context().theme_scope();
                                            subscription = scope->capture(
                                                [&](theme_runtime::DirtyPhase) {
                                                    const auto owner = f->services.tree().mounted()[0];
                                                    f->services.destroy(
                                                        f->services.tree().snapshot(owner).rows[0].switcher);
                                                },
                                                [scope] { static_cast<void>(scope->tree_metrics()); });
                                        }
                                        Input(InputProps{}.defaultValue(u8"Alive"));
                                    }})});
                  }});
        }});
        sync(*f);
        const auto owner = f->services.tree().mounted()[0];
        if (mode == 0) {
            trigger.set(1);
        } else if (mode == 1) {
            auto next = std::make_unique<ThemeConfig>();
            next->tree.tokens.font_size = dp(18);
            config->set(*next);
        } else {
            const auto row = f->services.tree().snapshot(owner).rows[0];
            auto handlers = *f->services.interactions().require(row.interaction).focus_handlers;
            const auto original = handlers.state_changed;
            int selections = 0;
            handlers.state_changed = [&, original, row](FocusPresentation value) {
                if (original) {
                    original(value);
                }
                if (value.focus_visible) {
                    f->services.destroy(row.title);
                    ++selections;
                }
            };
            f->services.interactions().set_focus_handlers(row.interaction, std::move(handlers));
            check(f->services.focus().request_focus(row.interaction, FocusModality::pointer),
                  "focus callback fixture failed");
            key(*f, Key::enter);
            check(selections == 1, "keyboard presentation callback was not run exactly once");
        }
        sync(*f);
        check(f->services.tree().mounted().empty() && f->inputs.mounted_inputs().empty() &&
                  blocked_roots(f->services.interactions()).empty(),
              "reactive callback left private Tree resources");
        clean(*f);
    }
    for (bool private_child : {false, true}) {
        auto f = std::make_unique<Fixture>();
        InputRef ref;
        f->services.mount(Content{[&] {
            Tree(TreeProps{}.visible(false), {TreeItem{u8"a", u8"A"}.richTitle(TreeTitle{[&] {
                     Input(InputProps{}.ref(ref).onBlur([&] {
                         const auto owner = f->services.tree().mounted()[0];
                         f->services.destroy(private_child ? f->services.tree().snapshot(owner).rows[0].title : owner);
                     }));
                     check(ref.focus(), "mount final-boundary focus fixture failed");
                 }})});
            Text(u8"Surviving sibling");
        }});
        sync(*f);
        check(f->services.tree().mounted().empty() && f->services.components().component_count() == 1 && !ref.bound() &&
                  blocked_roots(f->services.interactions()).empty(),
              "final mount boundary accessed retired owner/private child");
        clean(*f);
    }
}

void theme_scene() {
    Fixture f;
    auto config = std::make_unique<Signal<ThemeConfig>>(ThemeConfig{});
    Signal<bool> visible{true};
    Signal<String> title{String{u8"Original title"}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                  Flex(FlexProps{}.vertical(true).gap(dp(7)), FlexContent{[&] {
                           Text(u8"Before");
                           Tree(TreeProps{}.visible(visible), {TreeItem{u8"a", title}.icon(TreeIcon{[] {
                                    Icon(IconProps{}.name(IconName::UserOutlined));
                                }})});
                           Text(u8"After");
                       }});
              }});
    }});
    sync(f);
    const auto id = f.services.tree().mounted()[0];
    const auto row = f.services.tree().snapshot(id).rows[0];
    const auto node = f.services.components().root(id);
    const auto count = f.services.components().component_count();
    const auto measured = f.nodes.require(node).measure_count;
    auto next = std::make_unique<ThemeConfig>();
    next->tree.tokens.background = Color::rgba8(5, 15, 25);
    next->tree.tokens.text = Color::rgba8(120, 40, 70);
    config->set(*next);
    sync(f);
    check(f.nodes.require(node).measure_count == measured, "Tree color-only update remeasured geometry");
    check(std::ranges::any_of(
              f.services.rounded_effects().packed_instances(),
              [&](const auto& e) { return e.material.visible && e.material.color == *next->tree.tokens.background; }),
          "Tree background override missed actual scene");
    const auto scope = f.services.components().theme_scope(id);
    const auto* old = &scope->snapshot();
    auto bad = std::make_unique<ThemeConfig>(*next);
    bad->tree.tokens.font_size = dp(100000000);
    bool rejected = false;
    try {
        config->set(*bad);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && old == &scope->snapshot(), "invalid Tree font was published before resolve/preflight");
    next->tree.tokens.font_size = dp(18);
    next->tree.tokens.indent_size = dp(20);
    config->set(*next);
    title.set(u8"Changed title");
    sync(f);
    check(f.services.components().component_count() == count && ink(f, row.title, bounds(f, row.title)) > 0 &&
              scope->snapshot().tree().metrics.font_size == 18,
          "Theme font recovery lost actual glyphs/identity");
    visible.set(false);
    sync(f);
    const auto texts = f.services.text().mounted_texts();
    check(std::abs(bounds(f, texts.back().component).y - bounds(f, texts.front().component).y -
                   bounds(f, texts.front().component).height - 7) < .01F,
          "hidden Tree retained an extra Flex gap");
    visible.set(true);
    sync(f);
    const auto b = bounds(f, row.component);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, b.x + b.width - 1, b.y + b.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == row.interaction,
          "pointer capture fixture failed");
    visible.set(false);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "hidden Tree retained pointer capture");
    clean(f);
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "expected Tree test case");
        const std::string_view name{argv[1]};
        if (name == "basic") {
            basic();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "rich") {
            rich();
        } else if (name == "scroll") {
            scroll();
        } else if (name == "clips") {
            clips();
        } else if (name == "boundaries") {
            boundaries();
        } else if (name == "mount_lifecycle") {
            mount_lifecycle();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "validation") {
            validation();
        } else if (name == "reactivity") {
            reactivity();
        } else if (name == "theme_scene") {
            theme_scene();
        } else {
            throw std::invalid_argument("unknown Tree test");
        }
        std::cout << "Tree " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
