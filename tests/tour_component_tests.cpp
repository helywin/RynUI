#include "component/tour_component.hpp"
#include "component/modal_component.hpp"
#include "component/popover_component.hpp"
#include "component/table_component.hpp"
#include "support/navigation_fixture.hpp"
#include <ryn/input.hpp>
#include <ryn/modal.hpp>
#include <ryn/popover.hpp>
#include <ryn/tour.hpp>
#include <ryn/config_provider.hpp>
#include <ryn/theme.hpp>
#include <ryn/table.hpp>

namespace {
using namespace ryn_test::navigation;

struct TourTableRecord final {
    int value{};
    friend bool operator==(const TourTableRecord&, const TourTableRecord&) = default;
};

void action(Fixture& f, runtime::ComponentId parent) {
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (descendant(f, button.component, parent)) {
            click(f, button.component);
            return;
        }
    }
    throw std::runtime_error("Tour action has no native button");
}

void retained_steps() {
    Fixture f;
    TourTargetRef target;
    InputRef editor;
    Signal<bool> open{true};
    Signal<std::size_t> current{0};
    Signal<bool> visible{true};
    Signal<std::vector<TourStep>> steps{
        std::vector<TourStep>{TourStep{u8"edit", u8"Edit"}.target(target).richDescription(TourDescription{
                                  [editor] { Input(InputProps{}.ref(editor).defaultValue(u8"retained draft")); }}),
                              TourStep{u8"missing", u8"No target"}.description(u8"Centered")}};
    bool accept = false;
    std::size_t changes = 0;
    f.services.mount(Content{[&] {
        TourTarget(target, TourTargetContent{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Target"); }}); }});
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"Background"); }});
        Tour(TourProps{}
                 .steps(steps)
                 .open(open)
                 .current(current)
                 .visible(visible)
                 .onChange([&](auto next) {
                     ++changes;
                     if (accept) {
                         current.set(next);
                     }
                 })
                 .onOpenChange([&](bool next) {
                     if (accept) {
                         open.set(next);
                     }
                 }));
    }});
    sync(f);
    const auto id = f.services.tour().mounted().front();
    auto view = f.services.tour().snapshot(id);
    check(view.visible && view.target && !view.centered && target.bound(),
          "Tour did not resolve its native retained target");
    check(view.target_bounds.width > 0 && view.target_bounds.height > 0, "Tour target geometry is a placeholder");
    const auto page = view.content;
    check(editor.focus() && editor.select(0, 8), "Tour rich editor could not receive native focus/selection");
    action(f, view.next);
    check(changes == 1 && f.services.tour().snapshot(id).current == 0, "Tour ignored controlled current refusal");
    action(f, view.close);
    check(f.services.tour().snapshot(id).visible, "Tour ignored controlled close refusal");
    accept = true;
    action(f, view.next);
    view = f.services.tour().snapshot(id);
    check(view.current == 1 && view.centered && !view.target, "missing target did not center the real step");
    action(f, view.previous);
    check(f.services.tour().snapshot(id).content == page && editor.focus() && editor.select(0, 8),
          "step navigation rebuilt a rich editor or discarded its ref/selection boundary");
    // Settle the deferred focus/geometry work from the preceding accepted navigation.
    sync(f);
    const auto before = f.services.interactions().input_constraint_diagnostics();
    const auto revision = f.services.frame_request_revision();
    for (int i = 0; i < 12; ++i) {
        sync(f);
    }
    if (f.services.interactions().input_constraint_diagnostics().publications != before.publications ||
        f.services.frame_request_revision() != revision || f.services.next_frame_deadline()) {
        std::cerr << "Tour idle: publications " << before.publications << " -> "
                  << f.services.interactions().input_constraint_diagnostics().publications << ", frame revisions "
                  << revision << " -> " << f.services.frame_request_revision() << ", deadline "
                  << bool(f.services.next_frame_deadline()) << '\n';
    }
    check(f.services.interactions().input_constraint_diagnostics().publications == before.publications &&
              f.services.frame_request_revision() == revision && !f.services.next_frame_deadline(),
          "static Tour rescheduled frames or republished policy");
    visible.set(false);
    sync(f);
    check(!f.services.tour().snapshot(id).visible &&
              f.services.interactions().input_constraint_diagnostics().active_leases == 0,
          "hidden Tour retained input authority");
    visible.set(true);
    sync(f);
    const auto resolved_target = f.services.tour().snapshot(id).target;
    check(resolved_target && f.services.destroy(*resolved_target), "Tour native target did not retire");
    sync(f);
    check(!target.bound() && f.services.tour().snapshot(id).centered,
          "retired native target retained a Tour binding/hole");
    f.services.set_window_active(false);
    sync(f);
    check(!f.services.tour().snapshot(id).visible && !f.services.next_frame_deadline(),
          "inactive Tour retained a frame schedule");
    f.services.set_window_active(true);
    sync(f);
    steps.set({TourStep{u8"missing", u8"Only remaining"}});
    sync(f);
    check(f.services.tour().snapshot(id).total == 1 && !editor.bound(), "removed dynamic step retained a live editor");
    action(f, f.services.tour().snapshot(id).next);
    check(!f.services.tour().snapshot(id).visible,
          "Finish did not close the uncontrolled request accepted by application");
    f.services.dispose();
    const auto cleanup = f.services.interactions().input_constraint_diagnostics();
    check(!target.bound() && cleanup.leases == 0 && cleanup.companions == 0 && f.services.interactions().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "Tour leave-page cleanup leaked native input/scene resources");
}

void popup_target_and_modal() {
    Fixture f;
    TourTargetRef target;
    InputRef editor;
    Signal<bool> popup{true};
    Signal<bool> open{false};
    Signal<bool> modal{false};
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.open(popup).trigger(PopoverTriggerMode::Manual),
                PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Popover owner"); }}); }},
                {{}, PopoverContent{[&] {
                     TourTarget(target, TourTargetContent{[editor] {
                                    Input(InputProps{}.ref(editor).defaultValue(u8"native target"));
                                }});
                 }}});
        Tour(TourProps{}.open(open).steps(std::vector<TourStep>{
            TourStep{u8"popup", u8"Popup target"}.target(target).richDescription(TourDescription{[modal] {
                Button(ButtonProps{}.onClick([modal] { modal.set(true); }),
                       ButtonContent{[] { Text(u8"Child modal"); }});
                Modal(ModalProps{}.open(modal).title(u8"Nested").onCancel([modal](auto) { modal.set(false); }),
                      ModalContent{[] { Input(InputProps{}.defaultValue(u8"nested editor")); }});
            }})}));
    }});
    sync(f);
    const auto popup_id = f.services.popover().mounted().front();
    open.set(true);
    sync(f);
    const auto id = f.services.tour().mounted().front();
    const auto view = f.services.tour().snapshot(id);
    check(view.visible && view.target && !view.centered && editor.focus(),
          "first Tour publication evicted its containing popup target");
    check(f.services.popover().snapshot(popup_id).geometry.visible, "Tour closed the target popup");
    modal.set(true);
    sync(f);
    check(f.services.modal().visible() && !f.services.interactions().component_active(view.panel),
          "Tour controls escaped the innermost Modal boundary");
    modal.set(false);
    sync(f);
    check(f.services.interactions().component_active(view.panel), "child Modal close did not return Tour authority");
    popup.set(false);
    sync(f);
    check(f.services.tour().snapshot(id).centered && !f.services.tour().snapshot(id).target,
          "closed target popup kept stale geometry/admission");
    f.services.dispose();
    const auto diagnostics = f.services.interactions().input_constraint_diagnostics();
    check(diagnostics.leases == 0 && diagnostics.companions == 0 && f.inputs.mounted_inputs().empty(),
          "nested Tour/Popover/Modal cleanup leaked");
}

void callback_lifetime() {
    for (const bool throwing : {false, true}) {
        Fixture f;
        f.services.mount(Content{[&] {
            Tour(TourProps{}
                     .defaultOpen()
                     .steps(std::vector<TourStep>{TourStep{u8"1", u8"First"}, TourStep{u8"2", u8"Second"}})
                     .onChange([&](auto) {
                         if (throwing) {
                             throw std::runtime_error("expected callback failure");
                         }
                         f.services.dispose();
                     }));
        }});
        sync(f);
        const auto id = f.services.tour().mounted().front();
        bool threw = false;
        try {
            action(f, f.services.tour().snapshot(id).next);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == throwing && !f.services.components().contains(id),
              "Tour callback destruction/throw failed to retire its owner");
        f.services.dispose();
        const auto diagnostics = f.services.interactions().input_constraint_diagnostics();
        check(diagnostics.leases == 0 && diagnostics.companions == 0, "failed Tour callback retained input leases");
    }
}

void theme_and_foreign_target() {
    const auto initial = resolve_theme();
    check(initial.tour().width == 520 && initial.tour().z_index == 1070,
          "Tour defaults differ from the pinned native recipe");
    ThemeConfig custom;
    custom.tour.tokens.width = dp(360);
    custom.tour.tokens.highlight = Color::rgba8(220, 30, 40);
    const auto scope = theme_runtime::ThemeScope::create_default();
    int notifications = 0;
    auto subscription = scope->capture([&](auto) { ++notifications; }, [&] { static_cast<void>(scope->tour()); });
    check(scope->update(custom) && notifications == 1 && scope->snapshot().tour().width == 360,
          "Tour typed Theme subscription/override failed");
    const auto inherited = resolve_theme({}, &scope->snapshot());
    check(inherited.tour() == scope->snapshot().tour() &&
              inherited.diagnostic_json().find("\"tour\"") != std::string::npos,
          "Tour nested Theme/serialization omitted fields");
    const auto identity = scope->snapshot().identity();
    custom.tour.tokens.width = dp(0);
    bool rejected = false;
    try {
        scope->update(custom);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && scope->snapshot().identity() == identity, "invalid Tour Theme mutated a published snapshot");
    Fixture first;
    Fixture second;
    TourTargetRef target;
    first.services.mount(Content{[&] { TourTarget(target, TourTargetContent{[] { Text(u8"foreign target"); }}); }});
    sync(first);
    second.services.mount(Content{[&] {
        Tour(TourProps{}.defaultOpen().steps(std::vector<TourStep>{TourStep{u8"foreign", u8"Foreign"}.target(target)}));
    }});
    sync(second);
    const auto view = second.services.tour().snapshot(second.services.tour().mounted().front());
    check(view.visible && view.centered && !view.target, "cross-window Tour target bypassed its window capability");
    second.services.dispose();
    first.services.dispose();
}

void throwing_factory_preserves_target() {
    Fixture f;
    TourTargetRef target;
    f.services.mount(Content{[&] {
        TourTarget(target, TourTargetContent{[] { Input(InputProps{}.defaultValue(u8"unrelated target")); }});
        Tour(TourProps{}.defaultOpen().steps(
            std::vector<TourStep>{TourStep{u8"throw", u8"Failing step"}.target(target).richDescription(
                TourDescription{[] { throw std::runtime_error("expected Tour factory failure"); }})}));
    }});
    const auto id = f.services.tour().mounted().front();
    bool threw = false;
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && target.bound() && !f.services.components().contains(id),
          "Tour factory failure retired its external ordering target");
    f.services.dispose();
    const auto diagnostics = f.services.interactions().input_constraint_diagnostics();
    check(diagnostics.leases == 0 && diagnostics.companions == 0, "Tour factory failure leaked input leases");
}

bool label(Fixture& f, runtime::ComponentId owner, StringView value) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, owner) && std::as_const(f.scene).text_state(text.scene).content() == value) {
            return true;
        }
    }
    return false;
}

void live_locale_retains_page() {
    for (int source = 0; source < 3; ++source) {
        Fixture f;
        Signal<LocaleId> language{locales::zh_CN};
        Signal<std::size_t> current{0};
        InputRef editor;
        int factories = 0;
        ThemeConfig theme;
        theme.tour.tokens.width = dp(360);
        TourLocale explicit_locale;
        if (source == 2) {
            explicit_locale = {{}, {}, {}, {}};
        }
        f.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language).theme(theme), ConfigProviderContent{[&] {
                               auto props = TourProps{}.defaultOpen().current(current).steps(std::vector<TourStep>{
                                   TourStep{u8"first", u8"Literal"}.richDescription(TourDescription{[&] {
                                       ++factories;
                                       Input(InputProps{}.ref(editor).defaultValue(u8"retained draft"));
                                   }}),
                                   TourStep{u8"last", u8"Literal last"}});
                               if (source) {
                                   props.locale(explicit_locale);
                               }
                               Tour(std::move(props));
                           }});
        }});
        sync(f);
        const auto id = f.services.tour().mounted().front();
        const auto initial = f.services.tour().snapshot(id);
        check(initial.bounds.width == 360 && editor.focus() && editor.select(8, 1),
              "Tour locale fixture did not retain its Theme/editor input domain");
        check(label(f, initial.next, source == 0 ? String{u8"下一步"}.view() : explicit_locale.next.view()),
              "Tour locale did not distinguish inherited/explicit English/empty labels");
        const auto focus = f.services.focus().state().focused;
        const auto publications = f.services.interactions().input_constraint_diagnostics().publications;
        language.set(locales::en_US);
        sync(f);
        auto changed = f.services.tour().snapshot(id);
        check(changed.content == initial.content && factories == 1 && f.services.focus().state().focused == focus &&
                  f.services.interactions().input_constraint_diagnostics().publications == publications &&
                  label(f, changed.next, source == 2 ? String{}.view() : String{u8"Next"}.view()) &&
                  !f.services.next_frame_deadline(),
              "Tour locale rebuilt rich content, focus, input policy or scheduling");
        current.set(1);
        sync(f);
        changed = f.services.tour().snapshot(id);
        check(label(f, changed.next, source == 2 ? String{}.view() : String{u8"Finish"}.view()),
              "Tour final action did not consume its locale source");
        current.set(0);
        sync(f);
        check(f.services.tour().snapshot(id).content == initial.content && factories == 1 && editor.bound(),
              "Tour locale/navigation discarded its visited page cache");
        f.services.dispose();
        language.set(locales::zh_CN);
        const auto cleanup = f.services.interactions().input_constraint_diagnostics();
        check(!editor.bound() && cleanup.leases == 0 && cleanup.companions == 0,
              "Tour locale observer survived owner retirement");
    }
}

void getters_can_retire_owner() {
    for (int property = 0; property < 3; ++property) {
        Fixture f;
        const auto retire = [&] {
            f.services.dispose();
        };
        f.services.mount(Content{[&] {
            auto props = TourProps{}.defaultOpen().steps(std::vector<TourStep>{TourStep{u8"only", u8"Only"}});
            if (property == 0) {
                props.open(bind([&] {
                    retire();
                    return true;
                }));
            } else if (property == 1) {
                props.current(bind([&] {
                    retire();
                    return std::size_t{};
                }));
            } else {
                props.locale(bind([&] {
                    retire();
                    return TourLocale{};
                }));
            }
            Tour(std::move(props));
        }});
        check(f.services.components().component_count() == 0 && f.services.interactions().size() == 0,
              "Tour continued mounting after an application getter retired the owner");
        const auto cleanup = f.services.interactions().input_constraint_diagnostics();
        check(cleanup.leases == 0 && cleanup.companions == 0, "Tour retiring getter retained authority");
    }
}

void table_target_uses_retained_translation() {
    Fixture f;
    TourTargetRef target;
    Signal<bool> open{false};
    std::vector<TourTableRecord> records;
    for (int i = 0; i < 100; ++i) {
        records.push_back({i});
    }
    f.services.mount(Content{[&] {
        Table(
            TableProps<TourTableRecord>{}
                .dataSource(records)
                .rowKey([](const TourTableRecord& row) { return String::from_utf8(std::to_string(row.value)).value(); })
                .columns(
                    {TableColumn<TourTableRecord>{u8"target", u8"Target",
                                                  [target](TableCell<TourTableRecord> cell) {
                                                      if (cell.value().value == 0) {
                                                          TourTarget(target, TourTargetContent{[] {
                                                                         Input(InputProps{}.defaultValue(u8"target"));
                                                                     }});
                                                      } else {
                                                          Text(u8"Row");
                                                      }
                                                  }}
                         .width(240)
                         .fixed(TableFixed::Start),
                     TableColumn<TourTableRecord>{u8"scroll", u8"Scroll", [](auto) { Text(u8"Middle"); }}.width(800)})
                .virtualRows({60, 100, 2})
                .scrollY(180));
        Tour(TourProps{}.open(open).mask(false).steps(
            std::vector<TourStep>{TourStep{u8"row", u8"Row target"}.target(target)}));
    }});
    sync(f);
    open.set(true);
    sync(f);
    const auto id = f.services.tour().mounted().front();
    const auto initial = f.services.tour().snapshot(id);
    check(initial.visible && initial.target && initial.target_bounds.width > 0,
          "Tour did not admit its lazily mounted virtual Table target");
    const auto table = f.services.table().snapshot(f.services.table().mounted().front());
    check(f.services.dispatch_scroll({3, 0, initial.target_bounds.x + 10, initial.target_bounds.y + 10}),
          "Tour target did not preserve Table's actual wheel route");
    sync(f);
    const auto after = f.services.tour().snapshot(id);
    check(after.target == initial.target && after.target_bounds == initial.target_bounds &&
              f.services.table().snapshot(f.services.table().mounted().front()).scroll_x > table.scroll_x,
          "Tour read fixed-column geometry before Table's retained translation");
    f.services.dispose();
    const auto cleanup = f.services.interactions().input_constraint_diagnostics();
    check(!target.bound() && cleanup.leases == 0 && cleanup.companions == 0,
          "combined Tour/Table retirement leaked target/input ownership");
}
} // namespace

int main() {
    try {
        retained_steps();
        popup_target_and_modal();
        callback_lifetime();
        theme_and_foreign_target();
        throwing_factory_preserves_target();
        live_locale_retains_page();
        getters_can_retire_owner();
        table_target_uses_retained_translation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
