#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "component/popover_component.hpp"
#include "component/form_component.hpp"
#include "support/timeline_fixture.hpp"
#include <ryn/form.hpp>
#include <string_view>

namespace {
using namespace timeline_test;

void synchronize(Fixture& f, float width = 600, float height = 360) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "TreeSelect sync failed");
}

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
}

std::vector<TreeSelectItem> options() {
    return {TreeSelectItem{u8"root", u8"Root", {TreeSelectItem{u8"a", u8"Alpha"}, TreeSelectItem{u8"b", u8"Beta"}}}};
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().size() == 0 && f.inputs.editors().size() == 0 &&
              !f.inputs.sessions().active().valid() && blocked_roots(f.services.interactions()).empty() &&
              !f.services.next_frame_deadline(),
          "TreeSelect integration cleanup leaked resources");
}

void form() {
    Fixture f;
    FormField<String> field{String{u8"a"}};
    FormRef form_ref;
    TreeSelectRef ref;
    int changes = 0;
    std::optional<FormResult> result;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form_ref).onFinish([&](auto value) { result = std::move(value); }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"node", field).label(String{u8"Node"}).control(ref),
                          FormItemContent{[&] {
                              const auto change = field.onChange();
                              TreeSelect(TreeSelectProps{}
                                             .value(field.value())
                                             .onChange([&, change](String value) {
                                                 ++changes;
                                                 change(std::move(value));
                                             })
                                             .ref(ref)
                                             .treeDefaultExpandedKeys({u8"root"})
                                             .disabled(field.disabled())
                                             .readOnly(field.readOnly())
                                             .status(field.status()),
                                         options());
                          }});
             }});
    }});
    synchronize(f);
    const auto id = f.services.tree_select().mounted().front();
    const auto original = f.services.tree_select().snapshot(id);
    const auto editor = f.inputs.mounted_inputs().front().editor;
    field.set(u8"b");
    check(ref.focus(), "Form TreeSelect ref failed");
    synchronize(f);
    check(f.services.tree_select().snapshot(id).label == u8"Beta" && !f.inputs.sessions().active().valid(),
          "Form authority did not update display or readonly selector started IME");
    check(form_ref.reset(), "Form reset failed");
    synchronize(f);
    auto next = f.services.tree_select().snapshot(id);
    check(field.get() == u8"a" && next.value == u8"a" && next.label == u8"Alpha" && changes == 0 &&
              next.selector == original.selector && next.tree == original.tree &&
              f.inputs.mounted_inputs().front().editor == editor && f.services.focus().state().focused.has_value(),
          "focused Form reset lost authority, focus or retained identity");
    check(form_ref.reset() && form_ref.submit(), "same-value reset or submit failed");
    check(result && result->valid() && std::get<String>(result->values.front().value) == u8"a" && changes == 0,
          "Form submitted wrong typed value or reset emitted selection");
    clean(f);
}

void lifecycle() {
    for (int mode = 0; mode < 8; ++mode) {
        Fixture f;
        TreeSelectRef ref;
        runtime::ComponentId id;
        runtime::ComponentId parent;
        bool armed = false;
        const auto retire = [&] {
            if (!armed || !f.services.components().contains(id)) {
                return;
            }
            armed = false;
            if (mode == 3) {
                const auto tree = f.services.tree_select().snapshot(id).tree;
                f.services.destroy(tree);
                return;
            }
            f.services.destroy(id);
            if (mode == 4 || mode == 5) {
                f.services.append_slot(parent, Content{[&] { TreeSelect(TreeSelectProps{}.ref(ref), options()); }});
            }
        };
        f.services.mount(Content{[&] {
            auto& build = runtime::require_component_build_context();
            parent = build.mount_component<int>(0);
            const auto node = build.root(parent);
            f.layout.set_layout(node, layout::BoxLayout{{}, true});
            build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
            build.mount_slot(parent, Content{[&] {
                                 TreeSelect(TreeSelectProps{}
                                                .ref(ref)
                                                .treeDefaultExpandedKeys({u8"root"})
                                                .onChange([&](String) {
                                                    if (mode == 0 || mode == 3 || mode == 4) {
                                                        retire();
                                                    }
                                                })
                                                .onOpenChange([&](bool) {
                                                    if (mode == 1 || mode == 5) {
                                                        retire();
                                                    }
                                                })
                                                .onFocus([&] {
                                                    if (mode == 2) {
                                                        retire();
                                                    }
                                                })
                                                .onBlur([&] {
                                                    if (mode == 6 || mode == 7) {
                                                        retire();
                                                    }
                                                }),
                                            options());
                             }});
        }});
        synchronize(f);
        id = f.services.tree_select().mounted().front();
        armed = true;
        if (mode == 2) {
            static_cast<void>(ref.focus());
        } else if (mode == 6 || mode == 7) {
            static_cast<void>(ref.focus());
            if (mode == 6) {
                static_cast<void>(ref.blur());
            } else {
                f.services.set_window_active(false);
            }
        } else {
            static_cast<void>(ref.focus());
            key(f, Key::down);
            synchronize(f);
            if (f.services.components().contains(id)) {
                key(f, Key::right);
                key(f, Key::right);
                key(f, Key::enter);
            }
        }
        synchronize(f);
        check(!f.services.components().contains(id), "callback retirement left original owner alive");
        if (mode == 4 || mode == 5) {
            check(ref.bound() && ref.focus() && f.services.tree_select().mounted().size() == 1,
                  "old callback harmed replacement generation or shared ref");
        } else {
            check(!ref.bound(), "destroyed selector retained its ref binding");
        }
        clean(f);
    }
}

void rollback() {
    for (int mode = 0; mode < 5; ++mode) {
        Fixture f;
        TreeSelectRef ref;
        bool caught = false;
        f.services.mount(Content{[&] {
            try {
                auto props = TreeSelectProps{}.ref(ref);
                const auto fail_string = bind([]() -> String { throw std::runtime_error("original getter failure"); });
                if (mode == 0) {
                    props.value(fail_string);
                } else if (mode == 1) {
                    props.placeholder(fail_string);
                } else if (mode == 2) {
                    props.listHeight(-1);
                } else if (mode == 3) {
                    props.size(bind([]() -> ControlSize { throw std::runtime_error("original size failure"); }));
                } else {
                    props.defaultValue(u8"a").value(u8"b");
                }
                TreeSelect(props, options());
            } catch (const std::exception& e) {
                caught = true;
                check(mode >= 2 || std::string_view{e.what()} == "original getter failure",
                      "rollback replaced original exception");
            }
            Input(InputProps{}.placeholder(u8"Fallback"));
        }});
        synchronize(f);
        check(caught && !ref.bound() && f.services.tree_select().mounted().empty() &&
                  f.services.tree().mounted().empty() && f.services.popover().mounted().empty() &&
                  f.inputs.mounted_inputs().size() == 1 && f.inputs.editors().size() == 1 &&
                  blocked_roots(f.services.interactions()).empty(),
              "locally caught mount failure retained incomplete TreeSelect resources");
        clean(f);
    }
}

void positioning() {
    Fixture f;
    Signal<float> left{20};
    TreeSelectRef ref;
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}
                    .defaultOpen()
                    .trigger(PopoverTriggerMode::Manual)
                    .placement(PopoverPlacement::BottomLeft)
                    .layout(LayoutStyle{}.margin_left(bind([left] { return dp(left.get()); }))),
                PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Outer"); }}); }},
                PopoverSlots{.content = PopoverContent{[&] {
                                 TreeSelect(TreeSelectProps{}.ref(ref).defaultOpen().listHeight(96).layout(
                                                LayoutStyle{}.width(dp(220))),
                                            options());
                             }}});
    }});
    synchronize(f);
    const auto id = f.services.tree_select().mounted().front();
    auto original = f.services.tree_select().snapshot(id);
    const auto original_anchor = bounds(f, original.selector);
    check(original.open, "nested initial popup was not visible");
    left.set(75);
    synchronize(f);
    const auto next = f.services.tree_select().snapshot(id);
    const auto anchor = bounds(f, next.selector);
    check(std::abs((anchor.x - original_anchor.x) - (next.popup.x - original.popup.x)) < .01F &&
              std::abs(next.popup.x - anchor.x) < .01F,
          "nested TreeSelect used a stale ancestor anchor for one frame");
    check(ref.focus(), "nested selector ref failed");
    key(f, Key::down);
    synchronize(f);
    key(f, Key::escape);
    synchronize(f);
    check(!f.services.tree_select().snapshot(id).open &&
              f.services.popover().snapshot(f.services.popover().mounted().back()).geometry.visible,
          "child Escape closed parent Popover");
    clean(f);

    Fixture hidden;
    Signal<bool> parent_open{false};
    hidden.services.mount(Content{[&] {
        Popover(
            PopoverProps{}.open(parent_open).trigger(PopoverTriggerMode::Manual),
            PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Closed parent"); }}); }},
            PopoverSlots{.content = PopoverContent{[&] { TreeSelect(TreeSelectProps{}.defaultOpen(), options()); }}});
    }});
    synchronize(hidden);
    const auto hidden_id = hidden.services.tree_select().mounted().front();
    check(!hidden.services.tree_select().snapshot(hidden_id).open, "closed ancestor left child popup open");
    parent_open.set(true);
    synchronize(hidden);
    const auto hidden_snapshot = hidden.services.tree_select().snapshot(hidden_id);
    check(!hidden_snapshot.open, "opening a previously closed ancestor resurrected suppressed child");
    click(hidden, hidden_snapshot.selector);
    check(hidden.services.tree_select().snapshot(hidden_id).open, "explicit nested reopen failed");
    clean(hidden);
}

void matrix() {
    for (auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        for (float width : {280.0F, 104.0F}) {
            auto fixture = std::make_unique<Fixture>();
            auto& f = *fixture;
            auto config = std::make_unique<ThemeConfig>();
            config->algorithms = {algorithm};
            const auto selected_color = Color::rgba8(21, 76, 123);
            config->tree_select.tokens.tree.node_selected_background = selected_color;
            std::vector<TreeSelectItem> list;
            for (int i = 0; i < 12; ++i) {
                const auto value = String::from_utf8("Entry " + std::to_string(i)).value();
                list.emplace_back(value, value);
            }
            TreeSelectRef ref;
            f.services.mount(Content{[&] {
                Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                          TreeSelect(TreeSelectProps{}
                                         .ref(ref)
                                         .defaultValue(u8"Entry 0")
                                         .defaultOpen()
                                         .listHeight(96)
                                         .layout(LayoutStyle{}.margin_left(dp(20)).margin_top(dp(20)).width(dp(width))),
                                     list);
                      }});
            }});
            synchronize(f, 360, 240);
            const auto id = f.services.tree_select().mounted().front();
            auto s = f.services.tree_select().snapshot(id);
            const auto tree = f.services.tree().snapshot(s.tree);
            check(inside(s.popup, {0, 0, 360, 240}), "TreeSelect popup escaped finite viewport");
            bool glyph_seen = false;
            bool fill_seen = false;
            for (const auto& text : f.services.text().mounted_texts()) {
                if (!descendant(f, text.component, s.tree)) {
                    continue;
                }
                for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
                    for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                        const auto& glyph = f.scene.glyph_scene().instances().at(i);
                        const runtime::Rect clip{glyph.clip_bounds[0], glyph.clip_bounds[1],
                                                 glyph.clip_bounds[2] - glyph.clip_bounds[0],
                                                 glyph.clip_bounds[3] - glyph.clip_bounds[1]};
                        const runtime::Rect ink{glyph.position_size[0] + glyph.translation_opacity[0],
                                                glyph.position_size[1] + glyph.translation_opacity[1],
                                                glyph.position_size[2], glyph.position_size[3]};
                        check(inside(clip, s.popup), "actual TreeSelect glyph clip escaped popup");
                        const auto visible = graphics::intersect_effect_bounds(ink, clip);
                        if (visible.width > 0 && visible.height > 0 && glyph.translation_opacity[2] > 0) {
                            check(!f.scene.atlas().page_bytes(range.atlas_page).empty(), "glyph lacks R8 bytes");
                            glyph_seen = true;
                        }
                    }
                }
            }
            for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
                if (command.kind != graphics::SceneDrawKind::rounded_effect) {
                    continue;
                }
                for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
                    const auto& effect = f.services.rounded_effects().packed_instances()[i];
                    if (effect.material.color != selected_color || !effect.material.visible ||
                        effect.material.opacity <= 0) {
                        continue;
                    }
                    const auto& g = effect.geometry;
                    const runtime::Point center{g.shape.rect.x + g.translation.x + g.shape.rect.width / 2,
                                                g.shape.rect.y + g.translation.y + g.shape.rect.height / 2};
                    check(!g.ancestor_clip || inside(g.ancestor_clip->bounds, s.popup),
                          "selection fill clip escaped popup");
                    fill_seen |= graphics::rounded_effect_coverage(center, effect) > .8F &&
                                 (!g.ancestor_clip || contains(g.ancestor_clip->bounds, center));
                }
            }
            check(glyph_seen && fill_seen, "TreeSelect popup had no actual visible glyph or selection fill");
            for (const auto& hit : f.services.hit_test().records()) {
                const auto* record = f.services.interactions().find(hit.interaction);
                if (record && descendant(f, record->component, s.tree)) {
                    check(inside(hit.effective_clip, s.popup), "actual TreeSelect hit clip escaped popup");
                }
            }
            check(ref.focus(), "matrix selector ref failed");
            key(f, Key::down);
            synchronize(f, 360, 240);
            for (const auto boundary : {Key::home, Key::end}) {
                key(f, boundary);
                synchronize(f, 360, 240);
                std::array<float, 4> coverage{};
                const auto focus_color = f.services.components().theme_scope(s.tree)->snapshot().tree().colors.focus;
                for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
                    if (command.kind != graphics::SceneDrawKind::rounded_effect) {
                        continue;
                    }
                    for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
                        const auto& effect = f.services.rounded_effects().packed_instances()[i];
                        const auto& g = effect.geometry;
                        if (g.kind != graphics::RoundedEffectKind::outline || !effect.material.visible ||
                            effect.material.color != focus_color || effect.material.opacity <= 0) {
                            continue;
                        }
                        const runtime::Rect rect{g.shape.rect.x + g.translation.x, g.shape.rect.y + g.translation.y,
                                                 g.shape.rect.width, g.shape.rect.height};
                        const float gap = g.outline_offset + g.outline_width / 2;
                        const std::array<runtime::Point, 4> points{
                            {{rect.x - gap, rect.y + rect.height / 2},
                             {rect.x + rect.width / 2, rect.y - gap},
                             {rect.x + rect.width + gap, rect.y + rect.height / 2},
                             {rect.x + rect.width / 2, rect.y + rect.height + gap}}};
                        for (std::size_t edge = 0; edge < points.size(); ++edge) {
                            if (!g.ancestor_clip || contains(g.ancestor_clip->bounds, points[edge])) {
                                coverage[edge] = std::max(coverage[edge],
                                                          graphics::rounded_effect_coverage(points[edge], effect) *
                                                              effect.material.opacity * effect.material.color.alpha());
                            }
                        }
                    }
                }
                check(std::ranges::all_of(coverage, [](float value) { return value > .8F; }),
                      "TreeSelect boundary row lost actual focus edge coverage");
                const auto current = f.services.tree().snapshot(s.tree);
                check(current.rows.front().component == tree.rows.front().component &&
                          current.rows.back().component == tree.rows.back().component,
                      "scroll/reveal replaced retained Tree rows");
            }
            const auto scrolled = f.services.tree().snapshot(s.tree).scroll;
            check(scrolled > 0 &&
                      f.services.dispatch_scroll({0, 1, s.popup.x + s.popup.width / 2, s.popup.y + s.popup.height / 2}),
                  "TreeSelect wheel not owned");
            synchronize(f, 360, 240);
            check(f.services.tree().snapshot(s.tree).scroll < scrolled, "TreeSelect wheel did not scroll real Tree");
            static_cast<void>(f.frames.consume_request());
            const auto geometry = f.services.surfaces().diagnostics().geometry_updates;
            synchronize(f, 360, 240);
            check(!f.frames.pending() && !f.services.next_frame_deadline() &&
                      f.services.surfaces().diagnostics().geometry_updates == geometry,
                  "idle TreeSelect scheduled work or republished unchanged geometry");
            clean(f);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "form";
        if (name == "form") {
            form();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "rollback") {
            rollback();
        } else if (name == "positioning") {
            positioning();
        } else if (name == "matrix") {
            matrix();
        } else {
            throw std::runtime_error("unknown TreeSelect integration case");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
