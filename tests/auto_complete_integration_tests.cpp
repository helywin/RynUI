#include "support/auto_complete_fixture.hpp"
#include "component/popover_component.hpp"
#include "component/select_component.hpp"
#include "component/modal_component.hpp"
#include <cmath>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace auto_complete_test;

bool contained(runtime::Rect r, runtime::Rect clip) {
    return r.width <= 0 || r.height <= 0 ||
           (r.x >= clip.x - .01F && r.y >= clip.y - .01F && r.x + r.width <= clip.x + clip.width + .01F &&
            r.y + r.height <= clip.y + clip.height + .01F);
}

void clips() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    auto config = std::make_unique<ThemeConfig>();
    config->auto_complete.tokens.option_active_background = Color::rgba8(21, 76, 123);
    config->auto_complete.tokens.option_height = dp(44);
    config->auto_complete.tokens.popup_padding = dp(3);
    std::vector<AutoCompleteOption> list;
    for (int i = 0; i < 10; ++i) {
        const auto text = String::from_utf8("option-" + std::to_string(i)).value();
        list.push_back({text, text, text, false, AutoCompleteRichLabel{AutoCompleteOptionLabel{[text] {
                            Text(TypographyProps{}.content(text).mark(true));
                        }}}});
    }
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                  AutoComplete(AutoCompleteProps{}.options(list).defaultOpen().listHeight(105).layout(
                      LayoutStyle{}.width(dp(180))));
              }});
    }});
    sync(f, 220, 190);
    const auto id = f.services.auto_complete().mounted()[0];
    bool glyph = false;
    bool fill = false;
    std::vector<bool> seen(list.size());
    for (int step = 0; step < 5; ++step) {
        const auto s = snapshot(f, id);
        check(contained(s.bounds, {0, 0, 220, 190}), "popup escaped finite viewport");
        for (const auto& text : f.services.text().mounted_texts()) {
            if (!descendant(f, text.component, s.popup)) {
                continue;
            }
            const auto& primitive = f.scene.primitive(text.scene);
            for (const auto& range : primitive.draw_ranges) {
                const auto bytes = f.scene.atlas().page_bytes(range.atlas_page);
                for (auto i = range.instances.first; i < range.instances.first + range.instances.count; ++i) {
                    const auto& g = f.scene.glyph_scene().instances().at(i);
                    const runtime::Rect clip{g.clip_bounds[0], g.clip_bounds[1], g.clip_bounds[2] - g.clip_bounds[0],
                                             g.clip_bounds[3] - g.clip_bounds[1]};
                    check(contained(clip, s.clip), "actual glyph clip escaped menu");
                    if (clip.width <= 0 || clip.height <= 0 || g.translation_opacity[2] <= 0) {
                        continue;
                    }
                    check(!bytes.empty(), "glyph lacked actual R8 atlas bytes");
                    glyph = true;
                    for (std::size_t row = 0; row < s.rows.size(); ++row) {
                        if (descendant(f, text.component, s.rows[row])) {
                            seen[row] = true;
                        }
                    }
                }
            }
        }
        const auto mark = f.services.components().theme_scope(id)->snapshot().typography().colors.mark_background;
        const auto active = *config->auto_complete.tokens.option_active_background;
        for (const auto& q : f.services.surfaces().instances().instances()) {
            if (q.opacity <= 0 || q.color[3] <= 0) {
                continue;
            }
            if (q.color == std::array{mark.red(), mark.green(), mark.blue(), mark.alpha()} ||
                q.color == std::array{active.red(), active.green(), active.blue(), active.alpha()}) {
                fill = true;
                check(contained(
                          {q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]},
                          s.clip),
                      "actual mark/active fill escaped menu");
            }
        }
        for (const auto& h : f.services.hit_test().records()) {
            const auto* r = f.services.interactions().find(h.interaction);
            if (r && r->component != s.popup && descendant(f, r->component, s.popup)) {
                check(contained(h.effective_clip, s.clip), "actual hit escaped menu");
            }
        }
        check(f.services.dispatch_scroll({0, -1, s.clip.x + s.clip.width / 2, s.clip.y + s.clip.height / 2}),
              "menu wheel not owned");
        sync(f, 220, 190);
    }
    check(glyph && fill && std::ranges::all_of(seen, [](bool v) { return v; }),
          "scroll did not expose all retained rows/glyphs/fills");
    for (const auto extent : {runtime::Size{50, 72}, runtime::Size{1, 1}, runtime::Size{220, 190}}) {
        sync(f, extent.width, extent.height);
        const auto s = snapshot(f, id);
        check(contained(s.bounds, {0, 0, extent.width, extent.height}), "resized popup retained stale bounds");
        for (const auto& h : f.services.hit_test().records()) {
            const auto* r = f.services.interactions().find(h.interaction);
            if (r && descendant(f, r->component, s.popup)) {
                check(contained(h.effective_clip, {0, 0, extent.width, extent.height}), "resize retained stale hits");
            }
        }
    }
    clean(f);
}

void children() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> parent_open{true};
    Signal<bool> open{true};
    Signal<bool> child_open{false};
    Signal<bool> select_open{false};
    ButtonRef trigger;
    SelectRef select;
    AutoCompleteRichLabel popover{AutoCompleteOptionLabel{[&] {
        Popover(PopoverProps{}
                    .open(child_open)
                    .triggers(PopoverTriggers{false, false, true})
                    .content(u8"child popup")
                    .onOpenChange([&](bool v) { child_open.set(v); }),
                PopoverTrigger{[&] { Button(ButtonProps{}.ref(trigger), [] { Text(u8"Child"); }); }});
    }}};
    AutoCompleteRichLabel selector{AutoCompleteOptionLabel{[&] {
        Select(
            SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).ref(select).open(select_open).onOpenChange([&](bool v) {
                select_open.set(v);
            }));
    }}};
    auto config = std::make_unique<ThemeConfig>();
    config->auto_complete.tokens.option_height = dp(48);
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*config), ThemeContent{[&] {
                  Popover(PopoverProps{}.open(parent_open).onOpenChange([&](bool v) { parent_open.set(v); }),
                          PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Parent"); }); }},
                          {.content = PopoverContent{[&] {
                               AutoComplete(AutoCompleteProps{}
                                                .options({{u8"p", u8"p", u8"", false, popover},
                                                          {u8"s", u8"s", u8"", false, selector}})
                                                .open(open)
                                                .onOpenChange([&](bool v) { open.set(v); })
                                                .layout(LayoutStyle{}.width(dp(220))));
                           }}});
              }});
    }});
    sync(f, 800, 700);
    const auto id = f.services.auto_complete().mounted()[0];
    check(snapshot(f, id).visible && trigger.focus(), "child popup trigger missing");
    key(f, Key::enter);
    check(child_open.get() && snapshot(f, id).visible && parent_open.get(), "native child activation closed parent");
    const auto child = f.services.popover().mounted().front();
    const auto rect = f.services.popover().snapshot(child).geometry.bounds;
    const auto scroll = snapshot(f, id).scroll;
    check(f.services.dispatch_scroll({0, -1, rect.x + rect.width / 2, rect.y + rect.height / 2}),
          "child wheel missing");
    sync(f, 800, 700);
    check(snapshot(f, id).scroll == scroll, "child wheel scrolled parent menu");
    key(f, Key::escape);
    check(!child_open.get() && snapshot(f, id).visible && parent_open.get(),
          "Escape closed ancestor before child Popover");
    check(select.focus(), "child Select focus");
    select_open.set(true);
    sync(f, 800, 700);
    key(f, Key::escape);
    check(!select_open.get() && snapshot(f, id).visible && parent_open.get(),
          "Escape closed ancestor before child Select");
    clean(f);
}

void boundaries() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> open{true};
    Signal<bool> spinning{false};
    Signal<bool> modal{false};
    InputRef ref;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning),
             SpinContent{[&] { AutoComplete(AutoCompleteProps{}.options(options()).open(open).ref(ref)); }});
        Modal(ModalProps{}.open(modal), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    check(ref.focus(), "boundary focus");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "boundary preedit");
    spinning.set(true);
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus() && !f.inputs.sessions().active().owner.valid() &&
              !f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).composition().active,
          "Spin left popup/preedit/session");
    spinning.set(false);
    sync(f);
    check(!snapshot(f, id).visible, "Spin recovery resurrected refused popup");
    open.set(false);
    sync(f);
    open.set(true);
    sync(f);
    check(snapshot(f, id).visible, "Spin authority recovery");
    check(ref.focus(), "Modal fixture focus");
    modal.set(true);
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus(), "Modal left background popup/focus");
    modal.set(false);
    sync(f);
    open.set(false);
    sync(f);
    open.set(true);
    sync(f);
    check(snapshot(f, id).visible, "Modal recovery");
    const auto row = bounds(f, snapshot(f, id).rows[0]);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   row.x + row.width / 2, row.y + row.height / 2});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "row pointer capture fixture");
    f.services.components().set_branch_active(id, false);
    sync(f);
    check(!snapshot(f, id).visible && !ref.focus() && !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "hidden retained branch accepted input or retained capture");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                   row.x + row.width / 2, row.y + row.height / 2});
    check(snapshot(f, id).value.empty(), "late pointer release chose a hidden row");
    f.services.components().set_branch_active(id, true);
    sync(f);
    check(!snapshot(f, id).visible, "unhide resurrected refused popup");
    open.set(false);
    sync(f);
    open.set(true);
    sync(f);
    check(snapshot(f, id).visible, "unhide explicit recovery");
    f.services.set_window_active(false);
    sync(f);
    check(!snapshot(f, id).visible, "inactive window left popup");
    f.services.set_window_active(true);
    sync(f);
    check(!snapshot(f, id).visible, "window reactivate resurrected popup");
    clean(f);
}

void lifetime() {
    for (const std::string event : {"change", "search", "select", "clear", "open", "focus", "blur"}) {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        runtime::ComponentId parent;
        InputRef ref;
        runtime::ComponentId owner;
        f.services.mount(Content{[&] {
            auto& b = runtime::require_component_build_context();
            parent = b.mount_component<int>(0);
            const auto node = b.root(parent);
            f.layout.set_layout(node, layout::BoxLayout{});
            b.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
            Button(ButtonProps{}, [] { Text(u8"Foreign"); });
        }});
        sync(f);
        const auto foreign = f.buttons.mounted_buttons()[0].component;
        f.services.set_input_blocked(foreign, true);
        const auto baseline = blocked_roots(f.services.interactions());
        const auto count = f.services.components().component_count();
        bool armed = false;
        int retired = 0;
        auto replace = [&] {
            if (!armed) {
                return;
            }
            armed = false;
            ++retired;
            check(f.services.destroy(owner), "callback destroy failed");
            f.services.append_slot(
                parent, Content{[&] {
                    AutoComplete(AutoCompleteProps{}.defaultValue(u8"replacement").options(options()).ref(ref));
                }});
        };
        f.services.append_slot(parent, Content{[&] {
                                   AutoComplete(AutoCompleteProps{}
                                                    .defaultValue(u8"old")
                                                    .options(options())
                                                    .ref(ref)
                                                    .allowClear()
                                                    .defaultOpen(event != "open")
                                                    .onChange([&](String) {
                                                        if (event == "change") {
                                                            replace();
                                                        }
                                                    })
                                                    .onSearch([&](String) {
                                                        if (event == "search") {
                                                            replace();
                                                        }
                                                    })
                                                    .onSelect([&](String, AutoCompleteOption) {
                                                        if (event == "select") {
                                                            replace();
                                                        }
                                                    })
                                                    .onClear([&] {
                                                        if (event == "clear") {
                                                            replace();
                                                        }
                                                    })
                                                    .onOpenChange([&](bool) {
                                                        if (event == "open") {
                                                            replace();
                                                        }
                                                    })
                                                    .onFocus([&] {
                                                        if (event == "focus") {
                                                            replace();
                                                        }
                                                    })
                                                    .onBlur([&] {
                                                        if (event == "blur") {
                                                            replace();
                                                        }
                                                    }));
                               }});
        sync(f);
        owner = f.services.auto_complete().mounted()[0];
        const auto old_input = f.inputs.mounted_inputs()[0];
        const auto reset = detail::capture_form_input(ref).second;
        if (event != "focus") {
            check(ref.focus(), "callback fixture focus");
        }
        const auto stale = f.inputs.sessions().active();
        armed = true;
        if (event == "change" || event == "search") {
            edit(f, u8"requested");
        } else if (event == "select") {
            key(f, Key::enter);
        } else if (event == "open") {
            key(f, Key::down);
        } else if (event == "focus") {
            (void)ref.focus();
            sync(f);
        } else if (event == "blur") {
            (void)ref.blur();
            sync(f);
        } else {
            const auto old = snapshot(f, owner);
            bool clicked = false;
            for (auto hit : f.services.interactions().declaration_order()) {
                const auto* r = f.services.interactions().find(hit);
                if (r && r->eligible && !r->focusable && descendant(f, r->component, old.input)) {
                    click(f, r->component);
                    clicked = true;
                    break;
                }
            }
            check(clicked, "clear callback fixture");
        }
        check(retired == 1 && !f.services.components().contains(owner) && !f.inputs.editors().find(old_input.editor),
              "callback retained old generation");
        const auto replacement = f.services.auto_complete().mounted()[0];
        check(replacement != owner && ref.focus(), "generation rebind failed");
        const auto fresh = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"new preedit"}, {}, fresh})), "replacement preedit");
        reset();
        sync(f);
        check(f.inputs.editors().require(fresh.owner).composition().active &&
                  !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale})),
              "stale reset/session affected new generation");
        check(f.services.destroy(replacement), "replacement teardown");
        sync(f);
        check(blocked_roots(f.services.interactions()) == baseline &&
                  f.services.components().component_count() == count,
              "owned block cleanup removed foreign or leaked old generation");
        f.services.set_input_blocked(foreign, false);
        clean(f);
    }
}

void idle() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> open{true};
    Signal<std::vector<AutoCompleteOption>> suggestions{options()};
    InputRef ref;
    int child_blurs = 0;
    runtime::ComponentId owner;
    bool destroy = false;
    InputRef child;
    auto list = options();
    list[0].rich_label = AutoCompleteRichLabel{AutoCompleteOptionLabel{[&] {
        Input(InputProps{}.ref(child).onBlur([&] {
            ++child_blurs;
            if (destroy) {
                f.services.destroy(owner);
            }
        }));
    }}};
    suggestions.set(list);
    f.services.mount(Content{[&] { AutoComplete(AutoCompleteProps{}.options(suggestions).open(open).ref(ref)); }});
    sync(f);
    owner = f.services.auto_complete().mounted()[0];
    (void)f.frames.consume_request();
    const auto diagnostics = f.services.surfaces().diagnostics();
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.surfaces().diagnostics().geometry_updates == diagnostics.geometry_updates,
          "idle AutoComplete requested a frame or republished geometry");
    check(child.focus(), "child blur fixture");
    const auto stamp = f.inputs.sessions().active();
    destroy = true;
    suggestions.set({});
    sync(f);
    check(child_blurs == 1 && !f.services.components().contains(owner) && !child.bound() && !ref.bound() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})) &&
              blocked_roots(f.services.interactions()).empty(),
          "row blur destruction retained lifetime/block");
    clean(f);

    auto replacement_fixture = std::make_unique<Fixture>();
    auto& g = *replacement_fixture;
    InputRef reused;
    int blurs = 0;
    auto previous = options();
    previous[0].rich_label = AutoCompleteRichLabel{
        AutoCompleteOptionLabel{[&] { Input(InputProps{}.ref(reused).onBlur([&] { ++blurs; })); }}};
    Signal<std::vector<AutoCompleteOption>> dynamic{previous};
    g.services.mount(Content{[&] { AutoComplete(AutoCompleteProps{}.options(dynamic).defaultOpen()); }});
    sync(g);
    const auto control = g.services.auto_complete().mounted()[0];
    const auto row = snapshot(g, control).rows[0];
    check(reused.focus(), "rich replacement focus");
    const auto old_session = g.inputs.sessions().active();
    check(bool(g.inputs.dispatch(CompositionChanged{String{u8"old preedit"}, {}, old_session})),
          "rich replacement preedit");
    previous.resize(1);
    previous[0].rich_label = AutoCompleteRichLabel{
        AutoCompleteOptionLabel{[&] { Input(InputProps{}.defaultValue(u8"new child").ref(reused)); }}};
    dynamic.set(previous);
    sync(g);
    check(blurs == 1 && snapshot(g, control).rows == std::vector{row} && reused.bound() &&
              !g.inputs.editors().find(old_session.owner) &&
              !bool(g.inputs.dispatch(TextCommitted{String{u8"late"}, old_session})),
          "focused rich replacement/list shrink reused stale index, editor or row");
    clean(g);
}

void runtime_theme() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    auto config = std::make_unique<ThemeConfig>();
    config->auto_complete.tokens.option_height = dp(48);
    auto theme = std::make_unique<Signal<ThemeConfig>>(*config);
    InputRef child;
    int runs = 0;
    AutoCompleteRichLabel rich{AutoCompleteOptionLabel{[&] {
        ++runs;
        Input(InputProps{}.defaultValue(u8"Child native input").variant(InputVariant::Borderless).ref(child));
    }}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*theme), ThemeContent{[&] {
                  AutoComplete(
                      AutoCompleteProps{}
                          .options({{u8"child", u8"child", u8"", false, rich}, {u8"plain", u8"plain", u8"Plain label"}})
                          .defaultOpen()
                          .listHeight(25)
                          .layout(LayoutStyle{}.width(dp(180))));
              }});
    }});
    sync(f, 220, 190);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto original = snapshot(f, id);
    check(child.focus(), "focus clip fixture");
    sync(f, 220, 190);
    const auto verify_focus = [&] {
        const auto s = snapshot(f, id);
        bool focus = false;
        for (const auto& e : f.services.rounded_effects().packed_instances()) {
            if (e.geometry.kind == graphics::RoundedEffectKind::outline && e.material.visible &&
                e.material.opacity > 0 && e.geometry.shape.rect.y >= s.bounds.y) {
                focus = true;
                check(e.geometry.ancestor_clip && contained(e.geometry.ancestor_clip->bounds, s.clip),
                      "native child focus clip escaped menu");
            }
        }
        check(focus, "no actual native focus outline");
    };
    verify_focus();
    config->auto_complete.tokens.option_color = Color::rgba8(44, 55, 66);
    theme->set(*config);
    sync(f, 220, 190);
    check(snapshot(f, id).bounds == original.bounds, "color-only Theme changed popup geometry");
    config->algorithms = {ThemeAlgorithm::Dark};
    config->auto_complete.tokens.option_height = dp(60);
    config->auto_complete.tokens.font_size = dp(17);
    config->auto_complete.tokens.z_index_popup = 1250;
    theme->set(*config);
    sync(f, 220, 190);
    verify_focus();
    check(snapshot(f, id).input == original.input && snapshot(f, id).rows == original.rows && runs == 1 &&
              f.services.components().theme_scope(id)->snapshot().auto_complete().typography.font_size == 17,
          "runtime Theme remounted native/rich subtrees or dropped overrides");
    sync(f, 100, 95);
    verify_focus();
    clean(f);
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
        if (name == "clips") {
            clips();
        } else if (name == "children") {
            children();
        } else if (name == "boundaries") {
            boundaries();
        } else if (name == "lifetime") {
            lifetime();
        } else if (name == "idle") {
            idle();
        } else if (name == "theme_runtime") {
            runtime_theme();
        } else {
            throw std::runtime_error("unknown case");
        }
        std::cout << "AutoComplete integration " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
