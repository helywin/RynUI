#include "component/popover_component.hpp"
#include "component/tooltip_component.hpp"
#include "component/modal_component.hpp"
#include "support/timeline_fixture.hpp"
using namespace ryn;
using namespace timeline_test;

namespace {
int checks{};

void expect(bool value, const char* message) {
    ++checks;
    check(value, message);
}

struct Overlay final : Fixture {
    bool tooltip{};
    bool controlled{};
    Signal<bool> open{true};
    Signal<bool> disabled{false};
    Signal<bool> modal_open{false};
    std::array<InputRef, 3> editors;
    runtime::ComponentId owner;
    runtime::ComponentId legal;
    runtime::ComponentId sibling;
    runtime::ComponentId background;
    runtime::ComponentId trigger;
    input::InputConstraintLease restriction;
    int close_requests{};
    int activations{};

    void mount(bool is_tooltip, bool is_controlled, bool pending_timer = false) {
        tooltip = is_tooltip;
        controlled = is_controlled;
        const auto changed = [this](bool value) {
            if (!value) {
                ++close_requests;
            }
        };
        const auto content = [this] {
            Flex(FlexProps{}.vertical(true), FlexContent{[this] {
                     Input(InputProps{}.ref(editors[0]).defaultValue(u8"legal"));
                     Input(InputProps{}.ref(editors[1]).defaultValue(u8"sibling"));
                 }});
        };
        const auto entry = [this] {
            Button(ButtonProps{}.onClick([this] { ++activations; }), ButtonContent{[] { Text(u8"entry"); }});
        };
        services.mount(Content{[&] {
            Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                     if (tooltip) {
                         auto props =
                             TooltipProps{}
                                 .disabled(disabled)
                                 .placement(TooltipPlacement::Bottom)
                                 .trigger(pending_timer ? TooltipTriggerMode::Hover : TooltipTriggerMode::Manual)
                                 .onOpenChange(changed);
                         if (controlled) {
                             props.open(open);
                         } else {
                             props.defaultOpen(true);
                         }
                         Tooltip(props, TooltipTrigger{entry}, TooltipTitle{content});
                     } else {
                         auto props =
                             PopoverProps{}
                                 .disabled(disabled)
                                 .placement(PopoverPlacement::Bottom)
                                 .trigger(pending_timer ? PopoverTriggerMode::Hover : PopoverTriggerMode::Manual)
                                 .onOpenChange(changed);
                         if (controlled) {
                             props.open(open);
                         } else {
                             props.defaultOpen(true);
                         }
                         Popover(props, PopoverTrigger{entry}, PopoverSlots{.content = PopoverContent{content}});
                     }
                     Input(InputProps{}.ref(editors[2]).defaultValue(u8"background"));
                     Modal(ModalProps{}.open(modal_open), ModalContent{[] { Text(u8"actual independent Modal"); }});
                 }});
        }});
        sync(*this);
        sync(*this);
        owner = tooltip ? services.tooltip().mounted().front() : services.popover().mounted().front();
        const auto inputs = this->inputs.mounted_inputs();
        legal = inputs[0].component;
        sibling = inputs[1].component;
        background = inputs[2].component;
        trigger = buttons.mounted_buttons().front().component;
        expect(visible() && editors[0].focus(), "Real already-open overlay precondition failed");
        if (pending_timer) {
            // Tooltip gives an already keyboard-focused descendant immediate opening.
            // Exercise an actual delayed pointer intent without that independent shortcut.
            services.focus().clear_focus();
            const auto r = bounds(*this, trigger);
            services.pointer().dispatch({input::PointerIdentity::mouse(), input::PointerAction::move,
                                         input::PointerButton::none, r.x + r.width / 2, r.y + r.height / 2});
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(0)));
            expect(services.next_frame_deadline().has_value(), "Actual trigger did not schedule its retained timer");
        }
        restriction = services.create_input_constraint(services.interactions().input_root(owner),
                                                       input::InputConstraintKind::RestrictToSubtrees);
        auto batch = services.interactions().begin_input_constraint_update();
        batch.replace(restriction, {{services.interactions().input_root(legal)}, true});
        services.publish_input_constraints(batch);
        if (pending_timer) {
            expect(visible() && !services.next_frame_deadline() && close_requests == 0,
                   "Receiver-denied entry timer survived or closed a legitimate descendant producer");
        }
    }

    bool visible() {
        return tooltip ? services.tooltip().snapshot(owner).visible
                       : services.popover().snapshot(owner).geometry.visible;
    }

    void clean() {
        services.dispose();
        const auto d = services.interactions().input_constraint_diagnostics();
        expect(d.leases == 0 && d.companions == 0 && blocked_roots(services.interactions()).empty() &&
                   services.interactions().size() == 0 && services.surfaces().size() == 0 &&
                   !inputs.sessions().active().valid() && !services.next_frame_deadline(),
               "Overlay exact-generation resource/input cleanup failed");
    }
};

void run_descendant(bool tooltip, bool controlled) {
    Overlay f;
    f.mount(tooltip, controlled);
    expect(f.visible() && !f.services.interactions().component_active(f.owner) &&
               !f.services.interactions().component_active(f.trigger) && f.editors[0].focus() &&
               !f.editors[1].focus() && !f.editors[2].focus() && f.close_requests == 0,
           "Legal descendant maintenance granted owner/sibling or spuriously suppressed its producer");
    sync(f);
    const auto r = bounds(f, f.legal);
    const auto hit = f.services.hit_test().hit_test({r.x + r.width / 2, r.y + r.height / 2});
    const auto* record = hit ? f.services.interactions().find(*hit) : nullptr;
    expect(record && descendant(f, record->component, f.legal), "Legal popup editor lost actual pointer hit geometry");
    click(f, f.legal);
    const auto stamp = f.inputs.sessions().active();
    expect(stamp.valid() && bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 0}, stamp})) &&
               bool(f.inputs.dispatch(TextCommitted{String{u8"edited"}, stamp})),
           "Legal descendant did not receive real editor preedit/text");
    const auto boundary = f.services.interactions().input_boundary_revision();
    for (int i = 0; i < 12; ++i) {
        sync(f);
    }
    expect(f.visible() && f.close_requests == 0 && !f.services.next_frame_deadline() &&
               f.services.interactions().input_boundary_revision() == boundary,
           "Settled descendant maintenance republished policy, closed, or scheduled idle work");
    click(f, f.trigger);
    expect(f.activations == 0 && f.visible(), "Receiver-denied trigger activated through maintenance");
    if (controlled) {
        f.services.focus().dispatch({input::Key::escape, input::KeyAction::down});
        expect(f.visible() && f.close_requests == 1 && f.open.get(), "Controlled close refusal was lost");
        f.open.set(false);
        expect(!f.visible() && !f.inputs.sessions().active().valid(), "Accepted actual close retained popup input");
        expect(!f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp}), "Retired text epoch reached closed popup");
    }
    f.clean();
}

void authority(bool tooltip, int reason) {
    Overlay f;
    f.mount(tooltip, true);
    sync(f);
    input::InputConstraintLease block;
    if (reason == 0) {
        f.services.set_input_blocked(f.legal, true);
    } else if (reason == 1) {
        block = f.services.create_input_constraint(f.services.interactions().input_root(f.owner),
                                                   input::InputConstraintKind::BlockSubtrees);
        auto batch = f.services.interactions().begin_input_constraint_update();
        batch.replace(block, {{f.services.interactions().input_root(f.legal)}});
        f.services.publish_input_constraints(batch);
    } else if (reason == 2) {
        f.disabled.set(true);
    } else if (reason == 3) {
        f.services.components().set_branch_active(f.owner, false);
        f.services.synchronize_input_constraints();
        sync(f);
    } else if (reason == 4) {
        f.services.set_window_active(false);
    } else if (reason == 5) {
        f.modal_open.set(true);
    } else {
        bool rejected{};
        try {
            static_cast<void>(f.services.layout_and_synchronize({0, 0}, {}));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        expect(rejected && f.visible() && f.editors[0].focus(),
               "Invalid host viewport partially changed the accepted overlay domain");
        f.clean();
        return;
    }
    expect(!f.visible() && !f.editors[0].focus() && !f.inputs.sessions().active().valid(),
           "Maintenance bypassed genuine foreign block, disabled, hidden, or inactive authority");
    if (reason == 0) {
        f.services.set_input_blocked(f.legal, false);
    } else if (reason == 1) {
        f.services.release_input_constraint(block);
    } else if (reason == 2) {
        f.disabled.set(false);
    } else if (reason == 3) {
        f.services.components().set_branch_active(f.owner, true);
        f.services.synchronize_input_constraints();
    } else if (reason == 4) {
        f.services.set_window_active(true);
    } else if (reason == 5) {
        f.modal_open.set(false);
    }
    sync(f);
    expect(!f.visible(), "Removing a real suppression revived stale controlled popup");
    f.clean();
}
} // namespace

int main() {
    try {
        for (bool tooltip : {false, true}) {
            for (bool controlled : {false, true}) {
                run_descendant(tooltip, controlled);
            }
            for (int reason = 0; reason < 7; ++reason) {
                authority(tooltip, reason);
            }
            Overlay timer;
            timer.mount(tooltip, false, true);
            static_cast<void>(timer.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
            expect(timer.visible() && timer.close_requests == 0 && !timer.services.next_frame_deadline(),
                   "Cancelled trigger intent later closed or reopened its descendant producer");
            timer.clean();
        }
        std::cout << "Overlay descendant maintenance: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
