#include "support/auto_complete_fixture.hpp"
#include "component/select_component.hpp"
#include "component/popover_component.hpp"
#include "component/modal_component.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace auto_complete_test;

runtime::ComponentId region(Fixture& f) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    return id;
}

void first_mount() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    runtime::ComponentId owner;
    int closes = 0;
    f.services.mount(Content{[&] {
        owner = region(f);
        runtime::require_component_build_context().mount_slot(
            owner, Content{[&] {
                AutoComplete(AutoCompleteProps{}.options(
                    {{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                          Select(SelectProps{}.defaultOpen(true).options({{1, u8"One"}}).onOpenChange([&](bool value) {
                              if (!value) {
                                  ++closes;
                                  check(f.services.destroy(owner), "initial close destroy");
                              }
                          }));
                      }}}}}));
            }});
    }});
    check(closes == 1 && !f.services.components().contains(owner) && f.services.auto_complete().mounted().empty(),
          "first mount resumed a retired owner");
    clean(f);

    for (int target = 0; target < 3; ++target) {
        std::cout << "mount boundary " << target << '\n';
        auto boundary = std::make_unique<Fixture>();
        auto& current = *boundary;
        runtime::ComponentId old;
        int closed = 0;
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.options(
                {{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      old = current.services.auto_complete().mounted()[0];
                      Select(SelectProps{}.defaultOpen(true).options({{1, u8"One"}}).onOpenChange([&](bool value) {
                          if (value) {
                              return;
                          }
                          ++closed;
                          const auto state = snapshot(current, old);
                          const auto victim = target == 0   ? state.input
                                              : target == 1 ? state.popup
                                                            : current.services.components().children(state.rows[0])[0];
                          check(current.services.destroy(victim), "initial boundary private retirement");
                      }));
                  }}}}}));
        }});
        check(closed == 1 && !current.services.components().contains(old),
              "boundary resumed damaged private generation");
        sync(current);
        clean(current);
    }

    // A rich slot may retire any private generation, not just the public owner.
    for (int target = 0; target < 6; ++target) {
        std::cout << "mount rich retirement " << target << '\n';
        auto damaged = std::make_unique<Fixture>();
        auto& current = *damaged;
        runtime::ComponentId old;
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                {{u8"rich", u8"rich", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      old = current.services.auto_complete().mounted()[0];
                      const auto state = snapshot(current, old);
                      auto victim = old;
                      if (target == 1) {
                          victim = state.input;
                      }
                      if (target == 2) {
                          victim = state.popup;
                      }
                      if (target == 3) {
                          victim = *current.services.components().parent(state.rows[0]);
                      }
                      if (target == 4) {
                          victim = state.rows[0];
                      }
                      if (target == 5) {
                          victim = current.services.components().children(state.rows[0])[0];
                      }
                      check(current.services.destroy(victim), "rich private retirement");
                  }}}}}));
        }});
        check(!current.services.components().contains(old) && current.inputs.mounted_inputs().empty() &&
                  current.services.auto_complete().mounted().empty(),
              "damaged first mount was not retired");
        sync(current);
        clean(current);
    }

    // All externally read Props use the held connection Scope and exact private guard.
    for (int property = 0; property < 17; ++property) {
        std::cout << "mount Prop " << property << '\n';
        auto retired = std::make_unique<Fixture>();
        auto& current = *retired;
        runtime::ComponentId outer;
        int reads = 0;
        const auto retire = [&] {
            ++reads;
            if (property == 0) {
                check(current.services.destroy(outer), "layout reader retirement");
            } else {
                const auto id = current.services.auto_complete().mounted()[0];
                const auto state = snapshot(current, id);
                check(current.services.destroy(property % 2 ? state.input : state.popup), "Prop private retirement");
            }
        };
        current.services.mount(Content{[&] {
            outer = region(current);
            runtime::require_component_build_context().mount_slot(
                outer, Content{[&] {
                    AutoCompleteProps props;
                    if (property == 0) {
                        props.layout(LayoutStyle{}.width(bind([&] {
                            retire();
                            return dp(200);
                        })));
                    }
                    if (property == 1) {
                        props.options(bind([&] {
                            retire();
                            return options();
                        }));
                    }
                    if (property == 2) {
                        props.value(bind([&] {
                            retire();
                            return String{};
                        }));
                    }
                    if (property == 3) {
                        props.open(bind([&] {
                            retire();
                            return true;
                        }));
                    }
                    if (property == 4) {
                        props.filterOption(bind([&] {
                            retire();
                            return AutoCompleteFilter{};
                        }));
                    }
                    if (property == 5) {
                        props.defaultActiveFirstOption(bind([&] {
                            retire();
                            return true;
                        }));
                    }
                    if (property == 6) {
                        props.backfill(bind([&] {
                            retire();
                            return true;
                        }));
                    }
                    if (property == 7) {
                        props.allowClear(bind([&] {
                            retire();
                            return true;
                        }));
                    }
                    if (property == 8) {
                        props.disabled(bind([&] {
                            retire();
                            return false;
                        }));
                    }
                    if (property == 9) {
                        props.readOnly(bind([&] {
                            retire();
                            return false;
                        }));
                    }
                    if (property == 10) {
                        props.listHeight(bind([&] {
                            retire();
                            return 200.F;
                        }));
                    }
                    if (property == 11) {
                        props.placeholder(bind([&] {
                            retire();
                            return String{u8"placeholder"};
                        }));
                    }
                    if (property == 12) {
                        props.size(bind([&] {
                            retire();
                            return ControlSize::Large;
                        }));
                    }
                    if (property == 13) {
                        props.variant(bind([&] {
                            retire();
                            return InputVariant::Filled;
                        }));
                    }
                    if (property == 14) {
                        props.status(bind([&] {
                            retire();
                            return InputStatus::Warning;
                        }));
                    }
                    if (property == 15) {
                        props.options(options()).filterOption(
                            AutoCompleteFilter{[&](StringView, const AutoCompleteOption&) {
                                retire();
                                return true;
                            }});
                    }
                    if (property == 16) {
                        props.layout(LayoutStyle{}.width(bind([&] {
                            if (reads++ == 1) {
                                check(current.services.destroy(outer), "layout observer retirement");
                            }
                            return dp(200);
                        })));
                    }
                    AutoComplete(props);
                }});
        }});
        check(reads == (property == 16 ? 2 : 1) && current.services.auto_complete().mounted().empty() &&
                  current.inputs.mounted_inputs().empty(),
              "first Prop/filter callback resumed a private generation");
        sync(current);
        clean(current);
    }

    for (bool prefix : {false, true}) {
        std::cout << "mount affix " << prefix << '\n';
        auto affix = std::make_unique<Fixture>();
        auto& current = *affix;
        runtime::ComponentId old;
        const auto retire = [&] {
            old = current.services.auto_complete().mounted()[0];
            check(current.services.destroy(snapshot(current, old).input), "affix private retirement");
        };
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.options(options()),
                         prefix ? std::optional{AutoCompletePrefix{retire}} : std::nullopt,
                         !prefix ? std::optional{AutoCompleteSuffix{retire}} : std::nullopt);
        }});
        check(!current.services.components().contains(old), "affix callback resumed damaged private generation");
        sync(current);
        clean(current);
    }

    for (bool whole_owner : {false, true}) {
        auto focused = std::make_unique<Fixture>();
        auto& current = *focused;
        runtime::ComponentId old;
        int focuses = 0;
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.options(options()).autoFocus().onFocus([&] {
                ++focuses;
                old = current.services.auto_complete().mounted()[0];
                check(current.services.destroy(whole_owner ? old : snapshot(current, old).popup),
                      "initial autofocus retirement");
            }));
        }});
        sync(current);
        check(focuses == 1 && !current.services.components().contains(old) && !current.services.focus().state().focused,
              "first autofocus resumed damaged private generation");
        clean(current);
    }

    for (bool whole_owner : {false, true}) {
        std::cout << "mount formatter " << whole_owner << '\n';
        auto formatted = std::make_unique<Fixture>();
        auto& current = *formatted;
        runtime::ComponentId old;
        int formats = 0;
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                {{u8"format", u8"format", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      old = current.services.auto_complete().mounted()[0];
                      Input(InputProps{}.defaultValue(u8"count").showCount().countFormatter([&](InputCountInfo) {
                          ++formats;
                          const auto main = snapshot(current, old).input;
                          check(current.services.destroy(whole_owner ? old : main), "first formatter retirement");
                          return String{u8"count"};
                      }));
                  }}}}}));
        }});
        sync(current);
        check(formats == 1 && !current.services.components().contains(old), "first formatter resumed damaged owner");
        clean(current);
    }

    {
        auto reopened = std::make_unique<Fixture>();
        auto& current = *reopened;
        Signal<bool> open{false};
        int echo_closes = 0;
        current.services.mount(Content{[&] {
            AutoComplete(AutoCompleteProps{}.open(open).options(
                {{u8"child", u8"child", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      Select(SelectProps{}.defaultOpen(true).options({{1, u8"One"}}).onOpenChange([&](bool value) {
                          if (!value) {
                              ++echo_closes;
                              open.set(true);
                          }
                      }));
                  }}}}}));
        }});
        sync(current);
        const auto id = current.services.auto_complete().mounted()[0];
        const auto state = snapshot(current, id);
        bool row_hit = false;
        for (const auto& hit : current.services.hit_test().records()) {
            const auto* record = current.services.interactions().find(hit.interaction);
            row_hit |= record && record->component == state.rows[0];
        }
        check(echo_closes == 1 && open.get() && state.visible && row_hit &&
                  current.services.interactions().component_active(state.rows[0]),
              "initial boundary open echo was overwritten by stale row blocks");
        check(current.scene.glyph_scene().instances().size() > 0, "boundary open echo lost actual glyphs");
        clean(current);
    }

    // A replacement mounted during the close callback must outlive only its own generation.
    {
        auto rebound = std::make_unique<Fixture>();
        auto& current = *rebound;
        runtime::ComponentId safe;
        runtime::ComponentId outer;
        runtime::ComponentId old;
        int delivered = 0;
        current.services.mount(Content{[&] {
            safe = region(current);
            outer = region(current);
            runtime::require_component_build_context().mount_slot(
                outer, Content{[&] {
                    AutoComplete(AutoCompleteProps{}.options(
                        {{u8"child", u8"child", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                              old = current.services.auto_complete().mounted()[0];
                              Select(
                                  SelectProps{}.defaultOpen(true).options({{1, u8"One"}}).onOpenChange([&](bool open) {
                                      if (open) {
                                          return;
                                      }
                                      ++delivered;
                                      check(current.services.destroy(outer), "rebind retire old owner");
                                      runtime::require_component_build_context().mount_slot(
                                          safe, Content{[] {
                                              AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                                                  {{u8"new", u8"new", u8"New"}}));
                                          }});
                                  }));
                          }}}}}));
                }});
        }});
        check(delivered == 1 && !current.services.components().contains(old) &&
                  current.services.auto_complete().mounted().size() == 1,
              "close callback rebound generation");
        const auto replacement = current.services.auto_complete().mounted()[0];
        sync(current);
        check(replacement != old && snapshot(current, replacement).visible &&
                  snapshot(current, replacement).rows.size() == 1,
              "first mount damaged replacement");
        clean(current);
    }

    {
        auto mutated = std::make_unique<Fixture>();
        auto& current = *mutated;
        Signal<std::vector<AutoCompleteOption>> source{std::vector<AutoCompleteOption>{}};
        Signal<String> value{String{}};
        Signal<bool> open{false};
        Signal<ControlSize> size{ControlSize::Middle};
        int mounts = 0;
        AutoCompleteRichLabel rich;
        rich = AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
            ++mounts;
            Text(u8"Retained rich label");
            source.set({{u8"rich", u8"rich", u8"", false, rich}, {u8"next", u8"next", u8"Next"}});
            value.set(u8"typed during mount");
            open.set(true);
            size.set(ControlSize::Large);
        }}};
        source.set({{u8"rich", u8"rich", u8"", false, rich}});
        current.services.mount(
            Content{[&] { AutoComplete(AutoCompleteProps{}.options(source).value(value).open(open).size(size)); }});
        sync(current);
        const auto id = current.services.auto_complete().mounted()[0];
        const auto state = snapshot(current, id);
        check(mounts == 1 && state.rows.size() == 2 && state.value == String{u8"typed during mount"} && state.visible &&
                  bounds(current, state.input).height == 40,
              "rich slot Prop changes were lost or remounted");
        check(current.scene.glyph_scene().instances().size() > 0, "mutated mount has no actual glyphs");
        clean(current);
    }
}

void tab_anchor() {
    for (bool reverse : {false, true}) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        InputRef child;
        f.services.mount(Content{[&] {
            Button(ButtonProps{}, [] { Text(u8"Before"); });
            AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                {{u8"native", u8"native", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      Input(InputProps{}.ref(child).defaultValue(u8"Rich editor"));
                  }}}}}));
            Button(ButtonProps{}, [] { Text(u8"After"); });
        }});
        sync(f);
        const auto id = f.services.auto_complete().mounted()[0];
        click(f, f.inputs.mounted_inputs()[1].component);
        check(f.services.focus().state().focused == f.inputs.mounted_inputs()[1].interaction, "rich pointer focus");
        f.services.focus().dispatch({Key::tab, KeyAction::down, reverse ? KeyModifier::shift : KeyModifier::none});
        const auto expected = f.buttons.mounted_buttons()[reverse ? 0 : 1].interaction;
        check(f.services.focus().state().focused == expected && !snapshot(f, id).visible,
              "rich Tab/ShiftTab did not traverse relative to the main Input");
        clean(f);
    }
    for (int exit = 0; exit < 3; ++exit) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        InputRef nested;
        ButtonRef after;
        f.services.mount(Content{[&] {
            Button(ButtonProps{}, [] { Text(u8"Before"); });
            AutoComplete(AutoCompleteProps{}.defaultOpen().options(
                {{u8"popup", u8"popup", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
                      Popover(PopoverProps{}.defaultOpen(),
                              PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Nested"); }); }},
                              {.content = PopoverContent{
                                   [&] { Input(InputProps{}.ref(nested).defaultValue(u8"Nested editor")); }}});
                  }}}}}));
            Button(ButtonProps{}.ref(after), [] { Text(u8"After"); });
        }});
        sync(f);
        const auto id = f.services.auto_complete().mounted()[0];
        check(nested.focus(), "nested popup focus");
        sync(f);
        if (exit < 2) {
            f.services.focus().dispatch(
                {Key::tab, KeyAction::down, exit == 1 ? KeyModifier::shift : KeyModifier::none});
            check(f.services.focus().state().focused == f.buttons.mounted_buttons()[exit == 1 ? 0 : 2].interaction,
                  "nested popup Tab did not use main Input anchor");
        } else {
            check(after.focus(), "programmatic descendant exit");
        }
        sync(f);
        check(!snapshot(f, id).visible, "focus exit from nested popup left parent suggestions open");
        for (const auto& hit : f.services.hit_test().records()) {
            const auto* record = f.services.interactions().find(hit.interaction);
            check(!record || !descendant(f, record->component, snapshot(f, id).popup),
                  "exited popup retained actual hit");
        }
        clean(f);
    }
}

void zero_geometry() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<LogicalLength> height{dp(32)};
    Signal<LogicalLength> width{dp(200)};
    Signal<bool> open{true};
    InputRef ref;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}.options(options()).open(open).ref(ref).layout(
            LayoutStyle{}.width(width).height(height)));
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    click(f, snapshot(f, id).input);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "zero preedit fixture");
    height.set(dp(0));
    sync(f);
    check(bounds(f, id).height == 0, "zero height was not laid out");
    check(!snapshot(f, id).visible && !f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
              !f.inputs.editors().require(input.editor).composition().active && !ref.focus(),
          "zero allocated geometry retained popup/focus/session/preedit");
    const auto popup = snapshot(f, id).popup;
    for (const auto& hit : f.services.hit_test().records()) {
        const auto* record = f.services.interactions().find(hit.interaction);
        check(!record || !descendant(f, record->component, popup), "zero geometry retained actual popup hit");
    }
    for (const auto& text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, popup)) {
            continue;
        }
        for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
            for (auto index = range.instances.first; index < range.instances.first + range.instances.count; ++index) {
                check(std::ranges::none_of(f.services.scene_composer().ordered_scene().commands(),
                                           [&](const auto& command) {
                                               return command.kind == graphics::SceneDrawKind::glyph &&
                                                      index >= command.first_instance &&
                                                      index < command.first_instance + command.instance_count;
                                           }),
                      "zero geometry retained a submitted popup glyph");
            }
        }
    }
    const auto reset = detail::capture_form_input(ref).second;
    reset();
    check(!f.inputs.editors().require(input.editor).composition().active, "zero same-value reset restored preedit");
    height.set(dp(32));
    sync(f);
    check(!snapshot(f, id).visible, "geometry restore resurrected suppressed popup");
    open.set(false);
    sync(f);
    open.set(true);
    sync(f);
    check(snapshot(f, id).visible && ref.focus(), "zero recovery authority");
    const auto restored = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"width preedit"}, {}, restored})), "width preedit");
    width.set(dp(0));
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus() && !f.inputs.sessions().active().valid() &&
              !f.inputs.editors().require(input.editor).composition().active,
          "zero width retained edit state");
    width.set(dp(200));
    sync(f);
    check(!snapshot(f, id).visible && ref.focus(), "width recovery suppression/ref");
    key(f, Key::down);
    check(snapshot(f, id).visible, "explicit keyboard recovery after zero width");
    clean(f);

    auto modal = std::make_unique<Fixture>();
    auto& m = *modal;
    InputRef modal_input;
    m.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true), ModalContent{[&] {
                  AutoComplete(AutoCompleteProps{}.defaultOpen().options(options()).ref(modal_input));
              }});
    }});
    sync(m, 900, 700);
    const auto initial = m.services.auto_complete().mounted()[0];
    check(snapshot(m, initial).visible && !snapshot(m, initial).suppressed && modal_input.focus(),
          "initially open Modal/defaultOpen was mistaken for real zero geometry");
    clean(m);
}

void nested_anchor() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<LogicalLength> top{dp(0)};
    f.services.mount(Content{[&] {
        AutoComplete(
            AutoCompleteProps{}
                .defaultOpen()
                .listHeight(90)
                .layout(LayoutStyle{}.width(dp(220)).margin_top(top))
                .options({{u8"select", u8"select", u8"", false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[] {
                               Select(SelectProps{}.defaultOpen(true).options({{1, u8"One"}, {2, u8"Two"}}));
                           }}}},
                          {u8"two", u8"two", u8"Two"},
                          {u8"three", u8"three", u8"Three"},
                          {u8"four", u8"four", u8"Four"}}));
    }});
    sync(f, 700, 600);
    const auto control = f.services.auto_complete().mounted()[0];
    const auto child = f.services.select().mounted()[0];
    const auto verify = [&] {
        const auto select = f.services.select().snapshot(child);
        const auto anchor = bounds(f, select.selector);
        const auto& token = f.services.components().theme_scope(child)->snapshot().select();
        check(select.visible && std::abs(select.popup.x - anchor.x) < .01F &&
                  std::abs(select.popup.y - (anchor.y + anchor.height + token.metrics.gap)) < .01F,
              "root AutoComplete child Select used a stale/zero anchor in this frame");
        bool background = false;
        for (const auto& quad : f.services.surfaces().instances().instances()) {
            background |= quad.opacity > 0 && std::abs(quad.bounds[0] - select.popup.x) < .01F &&
                          std::abs(quad.bounds[1] - select.popup.y) < .01F &&
                          std::abs(quad.bounds[2] - select.popup.width) < .01F &&
                          std::abs(quad.bounds[3] - select.popup.height) < .01F;
        }
        check(background, "nested Select actual fill retained a stale anchor");
        bool glyphs = false;
        for (const auto& text : f.services.text().mounted_texts()) {
            if (!std::ranges::any_of(select.rows, [&](auto row) { return descendant(f, text.component, row); })) {
                continue;
            }
            for (const auto& range : f.scene.primitive(text.scene).draw_ranges) {
                for (auto index = range.instances.first; index < range.instances.first + range.instances.count;
                     ++index) {
                    const auto& glyph = f.scene.glyph_scene().instances().at(index);
                    if (glyph.translation_opacity[2] <= 0 || glyph.clip_bounds[2] <= glyph.clip_bounds[0] ||
                        glyph.clip_bounds[3] <= glyph.clip_bounds[1]) {
                        continue;
                    }
                    check(glyph.clip_bounds[0] >= select.popup.x && glyph.clip_bounds[1] >= select.popup.y &&
                              glyph.clip_bounds[2] <= select.popup.x + select.popup.width &&
                              glyph.clip_bounds[3] <= select.popup.y + select.popup.height,
                          "nested Select glyph clip retained a stale anchor");
                    glyphs = true;
                }
            }
        }
        check(glyphs, "nested Select has no actual visible glyphs");
        bool hits = false;
        for (const auto& hit : f.services.hit_test().records()) {
            const auto* record = f.services.interactions().find(hit.interaction);
            if (!record ||
                !std::ranges::any_of(select.rows, [&](auto row) { return descendant(f, record->component, row); })) {
                continue;
            }
            check(hit.effective_clip.x >= select.popup.x && hit.effective_clip.y >= select.popup.y &&
                      hit.effective_clip.x + hit.effective_clip.width <= select.popup.x + select.popup.width &&
                      hit.effective_clip.y + hit.effective_clip.height <= select.popup.y + select.popup.height,
                  "nested Select actual hit retained a stale anchor");
            hits = true;
        }
        check(hits, "nested Select has no actual hit records");
    };
    verify();
    top.set(dp(40));
    sync(f, 700, 600);
    verify();
    const auto menu = snapshot(f, control).clip;
    check(f.services.dispatch_scroll({0, -.25F, menu.x + menu.width - 1, menu.y + menu.height - 1}),
          "parent scroll fixture");
    sync(f, 700, 600);
    verify();
    clean(f);
}

void keyboard_modality() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    f.services.mount(
        Content{[&] { AutoComplete(AutoCompleteProps{}.options(options()).variant(InputVariant::Borderless)); }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    click(f, snapshot(f, id).input);
    check(f.services.focus().state().modality == FocusModality::pointer, "pointer modality fixture");
    f.services.focus().dispatch({Key::down, KeyAction::down});
    sync(f);
    check(f.services.focus().state().modality == FocusModality::keyboard && f.services.focus().state().focus_visible,
          "consumed Down did not deliver keyboard modality before keyup");
    bool outline = false;
    for (const auto& effect : f.services.rounded_effects().packed_instances()) {
        outline |= effect.geometry.kind == graphics::RoundedEffectKind::outline && effect.material.visible &&
                   effect.material.opacity > 0;
    }
    check(outline, "keydown frame has no actual focus outline");
    for (auto key : {Key::up, Key::enter, Key::escape}) {
        click(f, snapshot(f, id).input);
        f.services.focus().dispatch({key, KeyAction::down});
        check(f.services.focus().state().modality == FocusModality::keyboard &&
                  f.services.focus().state().focus_visible,
              "consumed command did not deliver keydown keyboard focus");
        sync(f);
    }
    clean(f);

    for (int mode = 0; mode < 5; ++mode) {
        auto retired = std::make_unique<Fixture>();
        auto& current = *retired;
        Signal<bool> disabled{false};
        Signal<bool> read_only{false};
        InputRef reference;
        runtime::ComponentId safe;
        runtime::ComponentId old;
        runtime::ComponentId old_input;
        bool delivered = false;
        bool armed = false;
        current.services.mount(Content{[&] {
            safe = region(current);
            AutoComplete(
                AutoCompleteProps{}
                    .options(options())
                    .defaultActiveFirstOption(false)
                    .disabled(disabled)
                    .readOnly(read_only)
                    .ref(reference)
                    .onOpenChange([&](bool open) {
                        if (!open || delivered || !armed) {
                            return;
                        }
                        delivered = true;
                        if (mode == 2) {
                            disabled.set(true);
                            return;
                        }
                        if (mode == 3) {
                            read_only.set(true);
                            return;
                        }
                        const auto state = snapshot(current, old);
                        check(current.services.destroy(mode == 0   ? old
                                                       : mode == 1 ? state.input
                                                                   : state.popup),
                              "keydown callback retirement");
                        current.services.append_slot(
                            safe, Content{[&] {
                                AutoComplete(
                                    AutoCompleteProps{}.options(options()).ref(reference).defaultActiveFirstOption(
                                        false));
                            }});
                    }));
        }});
        sync(current);
        old = current.services.auto_complete().mounted()[0];
        old_input = snapshot(current, old).input;
        click(current, old_input);
        key(current, Key::escape);
        check(current.services.focus().focus_from_pointer(current.inputs.mounted_inputs()[0].interaction),
              "pointer modality before callback");
        armed = true;
        current.services.focus().dispatch({Key::down, KeyAction::down});
        sync(current);
        check(delivered, "keydown callback was not delivered");
        if (mode != 3) {
            check(!current.services.focus().state().focused, "keydown callback generation/disabled focus guard");
        }
        if (mode == 2 || mode == 3) {
            check(!snapshot(current, old).active && !snapshot(current, old).visible,
                  "keydown mutated model after ineligibility");
        } else {
            check(!current.services.components().contains(old), "keydown retained damaged original owner");
            const auto replacement = current.services.auto_complete().mounted()[0];
            check(replacement != old && !snapshot(current, replacement).active && reference.focus(),
                  "keydown touched replacement model/ref");
        }
        clean(current);
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "case required");
        const std::string name = argv[1];
        if (name == "mount") {
            first_mount();
        } else if (name == "tab") {
            tab_anchor();
        } else if (name == "zero") {
            zero_geometry();
        } else if (name == "anchor") {
            nested_anchor();
        } else if (name == "modality") {
            keyboard_modality();
        } else {
            throw std::runtime_error("unknown case");
        }
        std::cout << "AutoComplete R1 " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
