#include "support/rate_fixture.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"

namespace {
using namespace rate_test;

void hidden_flow() {
    Fixture f;
    Signal<bool> visible{true};
    Signal<bool> disabled{false};
    Signal<int> count{5};
    int changes = 0;
    f.services.mount(Content{[&] {
        Text(u8"Before");
        Rate(RateProps{}.visible(visible).disabled(disabled).count(count).onChange([&](float) { ++changes; }));
        Text(u8"After");
    }});
    sync(f, {0, 0, 400, 300}, 9);
    const auto id = f.services.rate().mounted()[0];
    const auto original = f.services.rate().snapshot(id);
    const auto last = f.services.text().mounted_texts().back().component;
    const float with_rate = bounds(f, f.services.components().root(last)).y;
    pointer(f, original.items[1], PointerAction::move);
    pointer(f, original.items[1], PointerAction::down);
    check(f.services.pointer().state(PointerIdentity::mouse())->capture == original.items[1].interaction,
          "capture fixture failed");
    visible.set(false);
    sync(f, {0, 0, 400, 300}, 9);
    const float hidden_y = bounds(f, f.services.components().root(last)).y;
    check(std::abs(with_rate - hidden_y - 29) < .01F && !f.services.rate().snapshot(id).hover &&
              !f.services.focus().state().focused,
          "hidden Rate retained gap/hover/focus");
    no_capture(f);
    pointer(f, original.items[1], PointerAction::up);
    check(changes == 0, "hidden release committed");
    visible.set(true);
    sync(f);
    pointer(f, original.items[1], PointerAction::move);
    pointer(f, original.items[1], PointerAction::down);
    disabled.set(true);
    sync(f);
    no_capture(f);
    check(!f.services.rate().snapshot(id).hover && !f.services.focus().state().focused,
          "disabled retained temporary input");
    disabled.set(false);
    count.set(0);
    sync(f, {0, 0, 400, 300}, 9);
    check(std::abs(bounds(f, f.services.components().root(last)).y - hidden_y) < .01F, "count=0 retained flow/gap");
    count.set(7);
    sync(f);
    auto grown = f.services.rate().snapshot(id);
    check(grown.items.size() == 7 && grown.items[1].component == original.items[1].component,
          "growth remounted retained items");
    count.set(2);
    sync(f);
    count.set(7);
    sync(f);
    check(f.services.rate().snapshot(id).items[6].component == grown.items[6].component,
          "regrowth lost appended identity");
    const auto surfaces = f.services.surfaces().size();
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() && f.services.surfaces().size() == surfaces,
          "idle Rate scheduled work");
    count.set(0);
    sync(f);
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "empty Rate spins");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "owned blocks leaked on dispose");
}

void gates_and_rich_content() {
    Fixture f;
    Signal<bool> spin{false};
    Signal<bool> modal{false};
    Signal<bool> read_only{false};
    Signal<bool> shown{true};
    Signal<int> count{5};
    Signal<ThemeConfig> theme{*std::make_unique<ThemeConfig>()};
    InputRef ref;
    int title_runs = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Spin(SpinProps{}.spinning(spin), SpinContent{[&] {
                           Rate(RateProps{}.visible(shown).readOnly(read_only).count(count).allowHalf().tooltips(
                               {RateTooltip{TooltipTitle{[&] {
                                   ++title_runs;
                                   Input(InputProps{}.defaultValue(u8"Initial").ref(ref));
                               }}}}));
                       }});
                  Modal(ModalProps{}.open(modal).restoreFocusAfterClose(false),
                        ModalContent{[] { Text(u8"Boundary"); }});
              }});
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto original = f.services.rate().snapshot(id);
    pointer(f, original.items[0], PointerAction::move);
    sync(f);
    check(ref.focus({InputFocusCursor::All}), "rich title Input did not focus");
    const auto input = f.inputs.mounted_inputs()[0];
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Draft"}, f.inputs.sessions().active()})),
          "draft fixture failed");
    const auto session = f.inputs.sessions().active();
    read_only.set(true);
    sync(f);
    check(ref.focus() && f.inputs.sessions().active() == session, "readOnly swallowed native title Input");
    auto cfg = std::make_unique<ThemeConfig>();
    cfg->algorithms = {ThemeAlgorithm::Dark};
    cfg->rate.tokens.star_size = dp(23);
    theme.set(*cfg);
    sync(f);
    check(f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Draft" && f.inputs.sessions().active() == session &&
              title_runs == 1,
          "Theme reset rich identity/draft/ref/session");
    modal.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid() && !f.services.rate().snapshot(id).hover,
          "Modal gate retained rich input");
    modal.set(false);
    read_only.set(false);
    sync(f);
    pointer(f, original.items[0], PointerAction::move);
    sync(f);
    pointer(f, original.items[0], PointerAction::down);
    spin.set(true);
    sync(f);
    no_capture(f);
    check(!f.services.rate().snapshot(id).hover && !f.services.focus().state().focused,
          "Spin gate retained Rate interaction");
    spin.set(false);
    sync(f);
    pointer(f, original.items[0], PointerAction::move);
    sync(f);
    check(ref.focus(), "rich input did not recover");
    count.set(0);
    sync(f);
    check(!ref.focus(), "count shrink retained child input");
    count.set(5);
    shown.set(false);
    sync(f);
    check(!ref.focus(), "hidden rich input remained enabled");
    shown.set(true);
    sync(f);
    pointer(f, original.items[0], PointerAction::move);
    sync(f);
    check(ref.focus() && f.inputs.editors().require(input.editor).value() == "Draft" && title_runs == 1,
          "hide/count recovery remounted rich content");
    f.services.set_window_active(false);
    sync(f);
    no_capture(f);
    check(!f.services.rate().snapshot(id).hover && !f.services.focus().state().focused,
          "inactive window retained input");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "gate teardown leaked blocks");
}

void reentrant_generation() {
    Fixture f;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{});
        build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    runtime::ComponentId old;
    bool replace = false;
    const auto append = [&] {
        f.services.append_slot(parent, Content{[&] {
                                   Rate(RateProps{}.allowHalf().onHoverChange([&](auto value) {
                                       if (!value && replace) {
                                           replace = false;
                                           check(f.services.destroy(old), "hover callback teardown failed");
                                           f.services.append_slot(parent,
                                                                  Content{[] { Rate(RateProps{}.defaultValue(4)); }});
                                       }
                                   }));
                               }});
    };
    append();
    sync(f);
    old = f.services.rate().mounted()[0];
    const auto row = f.services.rate().snapshot(old).items[0];
    pointer(f, row, PointerAction::move);
    pointer(f, row, PointerAction::down);
    replace = true;
    f.services.set_input_blocked(parent, true);
    sync(f);
    const auto fresh = f.services.rate().mounted()[0];
    check(fresh != old && f.services.rate().snapshot(fresh).value == 4 && f.services.rate().mounted().size() == 1,
          "synchronous callback rebind retained stale generation");
    no_capture(f);
    check(std::ranges::find(blocked(f.services.interactions()), old) == blocked(f.services.interactions()).end(),
          "retired owned block leaked");
    f.services.set_input_blocked(parent, false);
    sync(f);
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "foreign parent block cleanup leaked");

    // onChange may destroy the same owner while its pointer route is active.
    Fixture g;
    runtime::ComponentId owner;
    g.services.mount(Content{[&] {
        Rate(RateProps{}.onChange([&](float) { check(g.services.destroy(owner), "change unmount failed"); }));
    }});
    sync(g);
    owner = g.services.rate().mounted()[0];
    click(g, g.services.rate().snapshot(owner).items[0]);
    check(g.services.rate().mounted().empty() && g.services.interactions().size() == 0 &&
              g.services.surfaces().size() == 0 && blocked(g.services.interactions()).empty(),
          "change unmount leaked resources");
}

void blur_ref_rebind() {
    Fixture f;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{});
        build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    Signal<bool> shown{true};
    InputRef shared_ref;
    runtime::ComponentId old;
    bool replace = false;
    f.services.append_slot(
        parent, Content{[&] {
            Rate(RateProps{}.visible(shown).tooltips({RateTooltip{TooltipTitle{[&] {
                Input(InputProps{}.ref(shared_ref).onBlur([&] {
                    if (replace) {
                        replace = false;
                        check(f.services.destroy(old), "native blur teardown failed");
                        f.services.append_slot(
                            parent, Content{[&] {
                                Rate(RateProps{}.readOnly().tooltips({RateTooltip{TooltipTitle{
                                    [&] { Input(InputProps{}.defaultValue(u8"Replacement").ref(shared_ref)); }}}}));
                            }});
                    }
                }));
            }}}}));
        }});
    sync(f);
    old = f.services.rate().mounted()[0];
    pointer(f, f.services.rate().snapshot(old).items[0], PointerAction::move);
    sync(f);
    check(shared_ref.focus(), "native blur fixture could not focus");
    const auto stale_session = f.inputs.sessions().active();
    replace = true;
    shown.set(false);
    sync(f);
    const auto fresh = f.services.rate().mounted()[0];
    check(fresh != old && f.services.rate().mounted().size() == 1 &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale_session})),
          "native blur rebind accepted stale generation/session");
    pointer(f, f.services.rate().snapshot(fresh).items[0], PointerAction::move);
    sync(f);
    check(shared_ref.focus() &&
              f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "Replacement",
          "old cleanup unbound replacement ref or modified draft");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "native blur rebind leaked owned blocks");
}
} // namespace

int main() {
    try {
        hidden_flow();
        gates_and_rich_content();
        reentrant_generation();
        blur_ref_rebind();
        std::cout << "Rate lifecycle passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
