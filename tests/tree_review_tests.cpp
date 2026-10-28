#include "component/tree_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "support/timeline_fixture.hpp"
#include <array>
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void sync_window(Fixture& f) {
    check(f.services.layout_and_synchronize({600, 280}, {0, 0, 600, 280}), "review layout failed");
}

void key(Fixture& f, Key k) {
    f.services.focus().dispatch({k, KeyAction::down});
    f.services.focus().dispatch({k, KeyAction::up});
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.components().component_count() == 0 && f.scene.size() == 0 &&
              f.scene.glyph_scene().instances().size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().size() == 0 && blocked_roots(f.services.interactions()).empty() &&
              !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline(),
          "review cleanup leaked scene/input/block resources");
}

struct MountFailure final {};

void mount_rollback() {
    for (int mode = 0; mode < 7; ++mode) {
        auto f = std::make_unique<Fixture>();
        Signal<int> trigger{0};
        InputRef ref;
        bool caught = false;
        int reads = 0;
        int fallback_clicks = 0;
        runtime::ComponentId retired;
        std::vector<runtime::NodeId> old_nodes;
        f->services.mount(Content{[&] {
            auto& outer = runtime::require_component_build_context();
            const auto survivor = outer.mount_component<int>(0);
            const auto survivor_node = outer.root(survivor);
            f->layout.set_layout(survivor_node, layout::BoxLayout{});
            outer.on_resource_cleanup(survivor, [&, survivor_node] { f->layout.remove_layout(survivor_node); });
            const auto remember = [&](auto&& self, runtime::NodeId node) -> void {
                old_nodes.push_back(node);
                for (auto child : f->nodes.require(node).children) {
                    self(self, child);
                }
            };
            const auto fail = [&] {
                retired = f->services.tree().mounted().front();
                remember(remember, f->services.components().root(retired));
                throw MountFailure{};
            };
            try {
                TreeProps props;
                props.visible(bind([&] {
                    static_cast<void>(trigger.get());
                    ++reads;
                    if (mode == 0) {
                        fail();
                    }
                    return true;
                }));
                if (mode == 3) {
                    props.layout(LayoutStyle{}.width(bind([&] {
                        static_cast<void>(trigger.get());
                        if (++reads == 2) {
                            fail(); // LayoutStyle's subscribe-time read, after its initial getter.
                        }
                        return dp(240);
                    })));
                }
                auto item = TreeItem{u8"a", u8"A"};
                if (mode == 2) {
                    item.icon(TreeIcon{[&] {
                        Input(InputProps{}.ref(ref));
                        fail();
                    }});
                } else {
                    item.richTitle(TreeTitle{[&] {
                            Input(InputProps{}.ref(ref).defaultValue(u8"Old"));
                            if (mode == 4 || mode == 5) {
                                retired = f->services.tree().mounted().front();
                                remember(remember, f->services.components().root(retired));
                                if (mode == 4) {
                                    f->services.destroy(retired);
                                } else {
                                    f->services.destroy(f->inputs.mounted_inputs().back().component);
                                }
                                outer.mount_slot(survivor, Content{[&] {
                                                     Tree({}, {TreeItem{u8"new", u8"New"}.richTitle(TreeTitle{[&] {
                                                              Input(
                                                                  InputProps{}.ref(ref).defaultValue(u8"Replacement"));
                                                          }})});
                                                 }});
                                throw MountFailure{};
                            }
                            if (mode == 6) {
                                retired = f->services.tree().mounted().front();
                                remember(remember, f->services.components().root(retired));
                                check(ref.focus(), "final coordination pre-layout ref must remain usable");
                                f->services.interactions().set_focus_handlers(
                                    f->inputs.mounted_inputs().back().interaction,
                                    FocusHandlers{[&](FocusPresentation value) {
                                        if (!value.focused) {
                                            throw MountFailure{};
                                        }
                                    }});
                            } else {
                                fail();
                            }
                        }})
                        .visible(mode != 6);
                }
                Tree(props, {item});
            } catch (const MountFailure&) {
                caught = true; // Deliberately catch within the same Content, then mount a real fallback.
            }
            Button(ButtonProps{}.wave(false).onClick([&] { ++fallback_clicks; }), [] { Text(u8"Fallback"); });
        }});
        const auto expected = mode == 4 || mode == 5 ? 1U : 0U;
        std::cout << "mount mode=" << mode << " caught=" << caught
                  << " tree_count=" << f->services.tree().mounted().size()
                  << " input_count=" << f->inputs.mounted_inputs().size() << std::endl;
        check(caught && f->services.tree().mounted().size() == expected &&
                  f->inputs.mounted_inputs().size() == expected,
              "R1.1: locally caught mount exception retained a partial Tree");
        check(!f->inputs.sessions().active().valid(), "failed mount retained an active Input session");
        for (auto node : old_nodes) {
            const bool live = f->nodes.find(node) != nullptr;
            const bool layout = f->layout.remove_layout(node);
            check(!live && !layout, "old mount node/layout survived rollback");
        }
        const int previous_reads = reads;
        trigger.set(1);
        check(reads == previous_reads, "failed mount retained a Prop subscription");
        sync(*f);
        click(*f, f->buttons.mounted_buttons().back().component);
        check(fallback_clicks == 1, "locally mounted fallback is not actually usable");
        if (expected) {
            check(f->services.tree().mounted()[0] != retired && ref.bound() && ref.focus() &&
                      f->inputs.editors().require(f->inputs.mounted_inputs()[0].editor).value() == "Replacement",
                  "rollback removed a replacement generation or same-ref binding");
        } else {
            check(!ref.bound() && blocked_roots(f->services.interactions()).empty(),
                  "rollback retained ref or input blocks");
        }
        clean(*f);
    }
}

void zero_area() {
    Fixture f;
    Signal<LogicalLength> width{dp(320)};
    Signal<LogicalLength> height{dp(180)};
    Signal<bool> popup_open{false};
    InputRef ref;
    int selections = 0;
    int checks = 0;
    f.services.mount(Content{[&] {
        Tree(TreeProps{}
                 .checkable()
                 .defaultExpandedKeys({u8"root"})
                 .onSelect([&](auto) { ++selections; })
                 .onCheck([&](auto) { ++checks; })
                 .layout(LayoutStyle{}.width(width).height(height)),
             {TreeItem{u8"root", u8"Root", {TreeItem{u8"rich", u8"Rich"}.richTitle(TreeTitle{[&] {
                           Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                    Input(InputProps{}.ref(ref).defaultValue(u8"Saved draft"));
                                    check(ref.focus(), "uncommitted initial zero bounds must not block a mounted ref");
                                    Select(SelectProps{}.options({{u8"a", u8"Alpha"}}).open(popup_open));
                                }});
                       }})}}});
        Button(ButtonProps{}.wave(false), [] { Text(u8"Foreign"); });
    }});
    sync(f);
    const auto owner = f.services.tree().mounted()[0];
    const auto first = f.services.tree().snapshot(owner);
    const auto editor = f.inputs.mounted_inputs()[0];
    const auto popup = f.services.select().mounted()[0];
    const auto foreign = f.buttons.mounted_buttons().back().component;
    f.services.set_input_blocked(foreign, true);
    const auto count = f.services.components().component_count();
    for (int cycle = 0; cycle < 2; ++cycle) {
        const auto zero = [&] {
            (cycle ? width : height).set(dp(0));
            sync(f);
        };
        const auto restore = [&] {
            width.set(dp(320));
            height.set(dp(180));
            sync(f);
        };
        check(f.services.focus().request_focus(first.rows[0].interaction, FocusModality::keyboard), "row focus failed");
        const int selected_before = selections;
        const int checked_before = checks;
        zero();
        key(f, Key::enter);
        key(f, Key::space);
        std::cout << "zero cycle=" << cycle << " selection_delta=" << selections - selected_before
                  << " check_delta=" << checks - checked_before
                  << " focused=" << f.services.focus().state().focused.has_value() << std::endl;
        check(selections == selected_before && checks == checked_before && !f.services.focus().state().focused,
              "R1.2: committed zero-area Tree still accepts focused keyboard input");
        restore();
        check(ref.focus({InputFocusCursor::All}), "restored rich Input ref failed");
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved draft"}, f.inputs.sessions().active()})),
              "draft fixture failed");
        const auto stamp = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 1}, stamp})),
              "composition fixture failed");
        zero();
        check(!ref.focus() && !f.inputs.sessions().active().valid() &&
                  !f.inputs.editors().require(editor.editor).composition().active &&
                  !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})),
              "zero area retained Input session/preedit/text admission");
        restore();
        popup_open.set(false);
        popup_open.set(true);
        sync(f);
        check(f.services.select().snapshot(popup).visible, "popup zero-area fixture failed");
        zero();
        check(!f.services.select().snapshot(popup).visible, "zero area retained a no-focus popup");
        restore();
        check(!f.services.select().snapshot(popup).visible, "size restoration resurrected old popup");
        const auto row = bounds(f, first.rows[0].component);
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                       row.x + row.width - 1, row.y + row.height / 2});
        check(f.services.pointer().state(PointerIdentity::mouse())->capture == first.rows[0].interaction,
              "capture fixture failed");
        zero();
        check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "zero area retained capture");
        restore();
        check(f.services.focus().request_focus(first.rows[0].interaction, FocusModality::keyboard),
              "restored row focus failed");
        key(f, Key::enter);
        check(selections == selected_before + 1 && ref.focus() &&
                  f.inputs.editors().require(editor.editor).value() == "Saved draft" &&
                  f.services.components().component_count() == count &&
                  f.services.tree().snapshot(owner).rows[1].title == first.rows[1].title &&
                  blocked_roots(f.services.interactions()) == std::vector{foreign},
              "zero-area restoration lost identity/draft/admission or changed foreign blocks");
    }
    height.set(dp(0));
    sync(f);
    f.services.destroy(owner);
    check(blocked_roots(f.services.interactions()) == std::vector{foreign}, "zero-area cleanup changed foreign blocks");
    f.services.set_input_blocked(foreign, false);
    clean(f);

    auto rebound = std::make_unique<Fixture>();
    runtime::ComponentId parent;
    rebound->services.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        parent = b.mount_component<int>(0);
        const auto node = b.root(parent);
        rebound->layout.set_layout(node, layout::BoxLayout{});
        b.on_resource_cleanup(parent, [&, node] { rebound->layout.remove_layout(node); });
    }});
    Signal<LogicalLength> shrinking{dp(80)};
    InputRef shared_ref;
    runtime::ComponentId old;
    bool replacing = false;
    rebound->services.append_slot(
        parent, Content{[&] {
            Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(240)).height(shrinking)),
                 {TreeItem{u8"old", u8"Old"}.richTitle(TreeTitle{[&] {
                     Input(InputProps{}.ref(shared_ref).onBlur([&] {
                         if (replacing) {
                             replacing = false;
                             rebound->services.destroy(old);
                             rebound->services.append_slot(
                                 parent, Content{[&] {
                                     Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(240)).height(dp(80))),
                                          {TreeItem{u8"new", u8"New"}.richTitle(TreeTitle{[&] {
                                              Input(InputProps{}.ref(shared_ref).defaultValue(u8"New generation"));
                                          }})});
                                 }});
                         }
                     }));
                 }})});
        }});
    sync(*rebound);
    old = rebound->services.tree().mounted()[0];
    check(shared_ref.focus(), "zero-area replacement focus fixture failed");
    const auto old_session = rebound->inputs.sessions().active();
    replacing = true;
    shrinking.set(dp(0));
    sync(*rebound);
    sync(*rebound);
    check(rebound->services.tree().mounted().size() == 1 && rebound->services.tree().mounted()[0] != old &&
              shared_ref.bound() && shared_ref.focus() && blocked_roots(rebound->services.interactions()).empty() &&
              !bool(rebound->inputs.dispatch(TextCommitted{String{u8"late"}, old_session})),
          "zero-area coordination touched a replacement generation/ref or retained an old block");
    clean(*rebound);
}

bool near(runtime::Rect a, runtime::Rect b) {
    return std::abs(a.x - b.x) < .01F && std::abs(a.y - b.y) < .01F && std::abs(a.width - b.width) < .01F &&
           std::abs(a.height - b.height) < .01F;
}

void translated_checkbox() {
    Fixture f;
    Signal<TreeKeys> checked{TreeKeys{}};
    std::vector<TreeItem> items;
    for (int i = 0; i < 10; ++i) {
        items.emplace_back(String::from_utf8(std::to_string(i)).value(), String{u8"Prefix row"});
    }
    items.push_back(TreeItem{u8"parent", u8"Parent", {TreeItem{u8"a", u8"Alpha"}, TreeItem{u8"b", u8"Beta"}}});
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).width(420).footer(ModalFooterMode::None).title(u8"Translated checkboxes"),
              ModalContent{[&] {
                  for (int i = 0; i < 12; ++i) {
                      Text(u8"Retained paragraph before the Tree");
                  }
                  Tree(TreeProps{}
                           .checkable()
                           .checkedKeys(checked)
                           .defaultExpandedKeys({u8"parent"})
                           .layout(LayoutStyle{}.width(dp(300)).height(dp(96))),
                       items);
              }});
    }});
    sync_window(f);
    const auto owner = f.services.tree().mounted()[0];
    const auto initial = f.services.tree().snapshot(owner);
    f.services.focus().request_focus(initial.rows[0].interaction, FocusModality::keyboard);
    check(f.services.focus().state().focused == initial.rows[0].interaction, "translated focus fixture failed");
    key(f, Key::end);
    sync_window(f);
    key(f, Key::up);
    key(f, Key::up);
    sync_window(f);
    sync_window(f);
    const auto modal = f.services.modal().snapshot(f.services.modal().mounted()[0]);
    const auto& row_node = f.nodes.require(f.services.components().root(initial.rows[10].component));
    std::cout << "translation_y=" << row_node.translation.y << " modal_scroll=" << modal.scroll
              << " tree_scroll=" << f.services.tree().snapshot(owner).scroll << " modal_max=" << modal.maximum_scroll
              << " body_yh=" << modal.body_bounds.y << ',' << modal.body_bounds.height
              << " tree_yh=" << bounds(f, owner).y << ',' << bounds(f, owner).height
              << " last_yh=" << bounds(f, initial.rows.back().component).y << ','
              << bounds(f, initial.rows.back().component).height << std::endl;
    check(modal.scroll > 0 && row_node.translation.y != 0 && f.services.tree().snapshot(owner).scroll > 0,
          "translated fixture did not exercise both parent and Tree scrolling");
    for (int mode = 0; mode < 3; ++mode) {
        checked.set(mode == 0 ? TreeKeys{} : mode == 1 ? TreeKeys{u8"a"} : TreeKeys{u8"parent"});
        sync_window(f);
        const auto box = bounds(f, initial.rows[10].checkbox);
        const auto t = f.services.components().theme_scope(owner)->snapshot().tree();
        const auto color = mode == 0 ? t.colors.checkbox_border : t.colors.checkbox_checked;
        const runtime::Point sample{box.x + box.width / 2, box.y + box.height / 2};
        int fills = 0;
        bool covered = false;
        bool half = false;
        for (const auto& e : f.services.rounded_effects().packed_instances()) {
            if (!e.material.visible || !e.geometry.ancestor_clip) {
                continue;
            }
            const auto rect = e.geometry.shape.rect;
            const runtime::Rect draw{rect.x + e.geometry.translation.x, rect.y + e.geometry.translation.y, rect.width,
                                     rect.height};
            const auto quad = graphics::rounded_effect_bounds(e, 0);
            if (e.material.color == color && near(draw, box)) {
                ++fills;
                check(inside(quad, box), "actual checkbox draw quad escaped its expected bounds");
                covered |= contains(e.geometry.ancestor_clip->bounds, sample) &&
                           graphics::rounded_effect_coverage(sample, e) > .9F;
                check(
                    inside(e.geometry.ancestor_clip->bounds, graphics::intersect_effect_bounds(box, modal.body_bounds)),
                    "actual checkbox fill clip escaped its translated partition");
            }
            if (mode == 1 && e.material.color == t.colors.checkbox_mark && draw.width == box.width / 2 &&
                std::abs(draw.x + draw.width / 2 - sample.x) < .01F &&
                std::abs(draw.y + draw.height / 2 - sample.y) < .01F) {
                half |= contains(e.geometry.ancestor_clip->bounds, sample) &&
                        graphics::rounded_effect_coverage(sample, e) > .9F;
            }
        }
        std::cout << "checkbox mode=" << mode << " actual_fills=" << fills << " center_covered=" << covered
                  << std::endl;
        check(fills > 0 && covered && (mode != 1 || half),
              "R1.3: translated checkbox has no correctly positioned actual fill/half-check coverage");
        check(f.services.hit_test().hit_test(sample) == initial.rows[10].interaction,
              "checkbox hit and visual positions diverged");
        if (mode == 2) {
            bool glyph = false;
            for (const auto& text : f.services.text().mounted_texts()) {
                if (!descendant(f, text.component, initial.rows[10].checkbox)) {
                    continue;
                }
                for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
                    for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                        const auto& g = f.scene.glyph_scene().instances().at(i);
                        if (g.translation_opacity[2] <= 0) {
                            continue;
                        }
                        const auto p = graphics::glyph_vertex(g, {.5F, .5F});
                        glyph |= contains(box, p) && p.x >= g.clip_bounds[0] && p.x <= g.clip_bounds[2] &&
                                 p.y >= g.clip_bounds[1] && p.y <= g.clip_bounds[3];
                    }
                }
            }
            check(glyph, "checked glyph draw did not agree with translated checkbox geometry");
        }
    }
    key(f, Key::home);
    sync_window(f);
    key(f, Key::end);
    sync_window(f);
    check(f.services.tree().snapshot(owner).rows[10].checkbox == initial.rows[10].checkbox,
          "scroll recovery remounted checkbox identity");
    clean(f);
}

void wheel_handoff() {
    Fixture f;
    Signal<LogicalLength> height{LogicalLength{}};
    Signal<bool> child_open{false};
    Signal<bool> foreign_open{false};
    std::vector<SelectOption> options;
    for (int i = 0; i < 30; ++i) {
        options.push_back({i, String{u8"Scrollable option"}});
    }
    std::vector<TreeItem> items{TreeItem{u8"first", u8"Chooser"}.richTitle(
        TreeTitle{[&] { Select(SelectProps{}.options(options).open(child_open).listHeight(64)); }})};
    for (int i = 0; i < 15; ++i) {
        items.emplace_back(String::from_utf8(std::to_string(i)).value(), String{u8"Natural height Tree row"});
    }
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).width(420).footer(ModalFooterMode::None).title(u8"Wheel handoff"),
              ModalContent{[&] {
                  Select(SelectProps{}.options(options).open(foreign_open).listHeight(64));
                  Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(300)).height(height)), items);
                  for (int i = 0; i < 8; ++i) {
                      Text(u8"Scrollable content after the Tree");
                  }
              }});
    }});
    sync_window(f);
    const auto owner = f.services.tree().mounted()[0];
    const auto modal = f.services.modal().mounted()[0];
    const auto row = f.services.tree().snapshot(owner).rows[0];
    const auto point = [&] {
        const auto r =
            graphics::intersect_effect_bounds(bounds(f, owner), f.services.modal().snapshot(modal).body_bounds);
        return runtime::Point{r.x + r.width - 4, r.y + std::min(r.height / 2, 16.0F)};
    };
    auto p = point();
    const auto before = f.services.modal().snapshot(modal);
    check(before.maximum_scroll > 0 && f.services.tree().snapshot(owner).scroll == 0 &&
              bounds(f, owner).height >= f.services.tree().snapshot(owner).content_height,
          "no-range natural Tree fixture failed");
    f.services.dispatch_scroll({0, -1, p.x, p.y});
    sync_window(f);
    std::cout << "natural Tree modal_scroll=" << before.scroll << " -> " << f.services.modal().snapshot(modal).scroll
              << std::endl;
    check(f.services.modal().snapshot(modal).scroll > before.scroll && f.services.tree().snapshot(owner).scroll == 0,
          "R1.4: no-range Tree consumed wheel instead of scrolling its Modal body");
    height.set(dp(64));
    sync_window(f);
    f.services.focus().clear_focus();
    p = point();
    f.services.dispatch_scroll({0, 100, p.x, p.y});
    sync_window(f);
    const float parent_scroll = f.services.modal().snapshot(modal).scroll;
    p = point();
    f.services.dispatch_scroll({0, -1, p.x, p.y});
    sync_window(f);
    check(f.services.tree().snapshot(owner).scroll > 0 && f.services.modal().snapshot(modal).scroll == parent_scroll,
          "Tree with its own range failed to consume wheel");
    check(f.services.tree().snapshot(owner).scroll <=
              f.services.tree().snapshot(owner).content_height - bounds(f, owner).height,
          "Tree scroll exceeded its actual committed range");
    p = point();
    f.services.dispatch_scroll({0, -100, p.x, p.y});
    sync_window(f);
    p = point();
    f.services.dispatch_scroll({0, -1, p.x, p.y});
    sync_window(f);
    check(f.services.modal().snapshot(modal).scroll > parent_scroll, "Tree end boundary failed inline wheel handoff");
    check(f.services.focus().request_focus(row.interaction, FocusModality::keyboard),
          "popup handoff focus fixture failed");
    sync_window(f);
    sync_window(f);
    for (int foreign = 0; foreign < 2; ++foreign) {
        child_open.set(false);
        foreign_open.set(false);
        (foreign ? foreign_open : child_open).set(true);
        sync_window(f);
        const auto select = f.services.select().mounted()[foreign ? 0 : 1];
        const auto popup = f.services.select().snapshot(select);
        check(popup.visible && !popup.rows.empty(), "popup scroll priority fixture failed");
        const float tree_scroll = f.services.tree().snapshot(owner).scroll;
        const float outer_scroll = f.services.modal().snapshot(modal).scroll;
        f.services.dispatch_scroll({0, -1, popup.popup.x + 10, popup.popup.y + 10});
        sync_window(f);
        check(f.services.select().snapshot(select).scroll > popup.scroll &&
                  f.services.tree().snapshot(owner).scroll == tree_scroll &&
                  f.services.modal().snapshot(modal).scroll == outer_scroll,
              "Tree/Modal consumed wheel through a child or foreign popup");
    }
    clean(f);
}

void focus_clips() {
    for (const auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        for (const float width : {180.0F, 56.0F}) {
            auto f = std::make_unique<Fixture>();
            auto config = std::make_unique<ThemeConfig>();
            config->algorithms = {algorithm};
            std::vector<TreeItem> items;
            for (int i = 0; i < 8; ++i) {
                const auto label = String::from_utf8("Entry " + std::to_string(i)).value();
                items.emplace_back(label, label);
            }
            f->services.mount(Content{[&] {
                Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                          Tree(
                              TreeProps{}.layout(
                                  LayoutStyle{}.margin_left(dp(20)).margin_top(dp(20)).width(dp(width)).height(dp(96))),
                              items);
                      }});
            }});
            sync_window(*f);
            const auto id = f->services.tree().mounted().front();
            const auto initial = f->services.tree().snapshot(id);
            const auto coverage = [&](bool require_visible = true) {
                std::array<float, 4> edges{};
                const auto color = f->services.components().theme_scope(id)->snapshot().tree().colors.focus;
                bool found{};
                for (const auto& command : f->services.scene_composer().ordered_scene().commands()) {
                    if (command.kind != graphics::SceneDrawKind::rounded_effect) {
                        continue;
                    }
                    for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
                        const auto& e = f->services.rounded_effects().packed_instances()[i];
                        const auto& g = e.geometry;
                        if (g.kind != graphics::RoundedEffectKind::outline || !e.material.visible ||
                            e.material.color != color || e.material.opacity <= 0) {
                            continue;
                        }
                        found = true;
                        const runtime::Rect r{g.shape.rect.x + g.translation.x, g.shape.rect.y + g.translation.y,
                                              g.shape.rect.width, g.shape.rect.height};
                        const float gap = g.outline_offset + g.outline_width / 2;
                        const std::array<runtime::Point, 4> points{{{r.x - gap, r.y + r.height / 2},
                                                                    {r.x + r.width / 2, r.y - gap},
                                                                    {r.x + r.width + gap, r.y + r.height / 2},
                                                                    {r.x + r.width / 2, r.y + r.height + gap}}};
                        for (std::size_t edge = 0; edge < points.size(); ++edge) {
                            if (!g.ancestor_clip || contains(g.ancestor_clip->bounds, points[edge])) {
                                edges[edge] = std::max(edges[edge], graphics::rounded_effect_coverage(points[edge], e) *
                                                                        e.material.opacity * e.material.color.alpha());
                            }
                        }
                    }
                }
                check(!require_visible || found, "focused Tree row has no actual outline");
                return edges;
            };
            const auto full = [&] {
                const auto edges = coverage();
                std::cout << "Tree focus width=" << width << " edges=" << edges[0] << ',' << edges[1] << ',' << edges[2]
                          << ',' << edges[3] << std::endl;
                check(std::ranges::all_of(edges, [](float value) { return value > .8F; }),
                      "Tree boundary row lost an actual focus-outline edge");
            };
            click(*f, initial.rows.front().title);
            key(*f, Key::home);
            sync_window(*f);
            check(f->services.focus().state().focused == initial.rows.front().interaction &&
                      f->services.focus().state().focus_visible,
                  "Home did not give first row keyboard focus");
            full();
            key(*f, Key::end);
            sync_window(*f);
            check(f->services.focus().state().focused == initial.rows.back().interaction &&
                      f->services.tree().snapshot(id).scroll > 0,
                  "End did not reveal last row");
            full();
            const auto tree_bounds = bounds(*f, id);
            for (const auto& hit : f->services.hit_test().records()) {
                const auto& record = f->services.interactions().require(hit.interaction);
                if (descendant(*f, record.component, id)) {
                    check(inside(hit.effective_clip, tree_bounds), "Tree focus padding expanded input clip");
                }
            }
            key(*f, Key::home);
            sync_window(*f);
            const auto first_row = bounds(*f, initial.rows.front().component);
            const auto metrics = f->services.components().theme_scope(id)->snapshot().tree().metrics;
            const runtime::Rect external{
                0, first_row.y + first_row.height + metrics.focus_offset + metrics.focus_width + 1, 600, 200};
            check(f->services.layout_and_synchronize({600, 280}, external), "external focus clip layout failed");
            const auto hidden = coverage(false);
            check(std::ranges::all_of(hidden, [](float value) { return value == 0; }),
                  "Tree row focus escaped an explicit document clip");
            sync_window(*f);
            click(*f, initial.rows.front().title);
            key(*f, Key::home);
            sync_window(*f);
            full();
            const auto final = f->services.tree().snapshot(id);
            check(final.rows.size() == initial.rows.size(), "focus padding changed topology");
            for (std::size_t i = 0; i < final.rows.size(); ++i) {
                check(final.rows[i].component == initial.rows[i].component &&
                          final.rows[i].interaction == initial.rows[i].interaction &&
                          final.rows[i].title == initial.rows[i].title,
                      "focus padding replaced retained row identity");
            }
            clean(*f);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "expected Tree R1 case");
        const std::string_view name{argv[1]};
        if (name == "mount_rollback") {
            mount_rollback();
        } else if (name == "zero_area") {
            zero_area();
        } else if (name == "translated_checkbox") {
            translated_checkbox();
        } else if (name == "wheel_handoff") {
            wheel_handoff();
        } else if (name == "focus_clips") {
            focus_clips();
        } else {
            throw std::invalid_argument("unknown Tree R1 case");
        }
        std::cout << "Tree R1 " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "original mount failure escaped its local Content catch\n";
        return 1;
    }
}
