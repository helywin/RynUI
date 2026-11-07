#include "gallery_i18n.hpp"
#include "gallery_pages_host.hpp"
#include "gallery_sample_source.hpp"
#include "component/select_component.hpp"
#include "component/selection_component.hpp"
#include "component/table_component.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace {
using namespace ryn;
using namespace rynui::example;
using ryn_test::i18n::require;

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPagesHost host{surfaces, inputs, frames};
    runtime::Size viewport{1440, 900};

    Fixture() {
        host.mount(viewport);
        sync();
    }

    void sync() {
        require(host.synchronize(viewport) && !host.snapshot().failed, "real Gallery page failed to synchronize");
    }

    void show(std::string_view identity) {
        require(host.request(host.model().find(identity)->route()), "Gallery route was rejected");
        static_cast<void>(host.process());
        sync();
    }

    bool has(String value) const {
        return std::ranges::any_of(services.text().mounted_texts(), [&](const auto& item) {
            return scene.text_state(item.scene).content().bytes() == value.bytes();
        });
    }

    bool has_fragment(std::string_view value) const {
        return std::ranges::any_of(services.text().mounted_texts(), [&](const auto& item) {
            return scene.text_state(item.scene).content().bytes().find(value) != std::string_view::npos;
        });
    }

    void reveal(runtime::ComponentId component) {
        const auto& node = nodes.require(services.components().root(component));
        const auto lane = host.snapshot().layout.document;
        host.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2,
                              -(node.bounds.y + node.translation.y - lane.y - lane.height / 3) / 48);
        sync();
    }

    void press(runtime::ComponentId component) {
        reveal(component);
        for (const auto& button : buttons.mounted_buttons()) {
            if (within(button.component, component)) {
                require(services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
                        "Gallery command focus failed");
                host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
                host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
                sync();
                return;
            }
        }
        throw std::runtime_error("Gallery command has no actual Button");
    }

    ryn::detail::MountedInputComponent editor(std::string_view identity) {
        const auto target = host.target(identity);
        require(target.valid(), "Gallery editor target is missing");
        reveal(target);
        for (const auto& item : inputs.mounted_inputs()) {
            if (within(item.component, target)) {
                return item;
            }
        }
        throw std::runtime_error("Gallery target has no actual editor");
    }

    void replace(const ryn::detail::MountedInputComponent& item, String value) {
        require(services.focus().request_focus(item.interaction, input::FocusModality::keyboard),
                "Gallery editor focus failed");
        host.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        require(bool(inputs.dispatch(input::TextCommitted{std::move(value), inputs.sessions().active()})),
                "Gallery editor commit failed");
        sync();
    }

    bool within(runtime::ComponentId child, runtime::ComponentId owner) const {
        for (auto id = std::optional{child}; id; id = services.components().parent(*id)) {
            if (*id == owner) {
                return true;
            }
        }
        return false;
    }

    void activate(std::string_view identity) {
        const auto owner = host.target(identity);
        for (const auto& button : buttons.mounted_buttons()) {
            if (within(button.component, owner)) {
                const auto& node = nodes.require(services.components().root(button.component));
                const auto x = node.bounds.x + node.translation.x + node.bounds.width / 2;
                const auto y = node.bounds.y + node.translation.y + node.bounds.height / 2;
                for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
                    host.dispatch(input::PointerInputEvent{input::PointerIdentity::mouse(), action,
                                                           input::PointerButton::primary, x, y});
                }
                sync();
                return;
            }
        }
        throw std::runtime_error("Gallery action has no actual Button");
    }
};

void declared_keys_schemas_and_fallback() {
    const auto& catalog = gallery_message_catalog();
    std::set<std::string> identities;
    for (const auto& declaration : gallery_messages()) {
        require(identities.insert(std::string{declaration.key.id().bytes()}).second, "duplicate Gallery message ID");
        MessageArgs args;
        for (const auto& parameter : declaration.key.schema()) {
            switch (parameter.type) {
            case MessageArgType::Text:
                args.push_back({parameter.name, String{u8"sample {literal}"}});
                break;
            case MessageArgType::Int:
                args.push_back({parameter.name, std::int64_t{-1}});
                break;
            case MessageArgType::UInt:
                args.push_back({parameter.name, std::uint64_t{18446744073709551615ULL}});
                break;
            case MessageArgType::Decimal:
                args.push_back({parameter.name, Decimal{"9007199254740993.125"}});
                break;
            }
        }
        for (const auto locale : {locales::zh_CN, locales::en_US}) {
            require(catalog.find(locale, declaration.key.id().view()), "Gallery declaration lacks an explicit locale");
            const std::array layers{catalog};
            const auto value = translate(LocaleCatalog::builtin(), locale, declaration.key, args, layers);
            const auto fallback = translate(LocaleCatalog::builtin(), locale, declaration.key, args);
            require(!value.empty() && value.bytes().find("gallery.") == std::string_view::npos &&
                        fallback.bytes().find("gallery.") == std::string_view::npos,
                    "message or missing-translation fallback leaked an automation key");
        }
    }
    for (const auto& sample : gallery_samples()) {
        require(identities.contains(std::string{sample.identity} + ".title"), "actual sample lacks a translated title");
        const auto* source = gallery_sample_source(sample.identity);
        require(source && !source->source_function.empty() && !source->public_header.empty(),
                "localization detached the real sample source");
    }
}

void real_shell_click_and_missing_catalog() {
    Fixture f;
    const auto before = f.host.snapshot();
    const auto mounts = f.host.work().page_mounts;
    require(f.host.locale() == locales::zh_CN && f.has(u8"组件总览"), "Gallery root does not start in Chinese");
    f.activate("gallery.shell.language");
    require(f.host.locale() == locales::en_US && f.has(u8"Component overview") &&
                f.host.snapshot().page == before.page && f.host.work().page_mounts == mounts,
            "actual language Button did not update the retained page");
    f.host.set_messages(MessageCatalog{});
    f.host.set_locale(locales::zh_CN);
    f.sync();
    require(f.has(u8"Component overview") && f.host.snapshot().page == before.page,
            "real Gallery with a missing catalog did not use readable declared fallbacks");
    f.host.set_messages(gallery_message_catalog());
    f.sync();
    require(f.has(u8"组件总览"), "real Gallery failed to recover its complete catalog");
}

void retained_page_editor_popup_and_retirement() {
    Fixture f;
    f.show("ant.component.input");
    const auto page = f.host.snapshot().page;
    const auto mounts = f.host.work().page_mounts;
    const auto runs = f.host.sample_runs("gallery.sample.input.basics");
    auto found = std::ranges::find_if(f.inputs.mounted_inputs(),
                                      [&](const auto& item) { return f.within(item.component, page); });
    require(found != f.inputs.mounted_inputs().end(), "real Input page has no editor");
    const auto input = *found;
    require(f.services.focus().request_focus(input.interaction, input::FocusModality::keyboard),
            "Gallery editor focus failed");
    auto& editor = f.inputs.editors().require(input.editor);
    require(static_cast<bool>(editor.replace_range({0, editor.value().size()}, "a中文z")),
            "Gallery draft setup failed");
    require(static_cast<bool>(editor.select({7, 1})), "reverse selection setup failed");
    require(static_cast<bool>(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "Gallery preedit setup failed");
    const auto draft = std::string{editor.value()};
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto offset = f.host.snapshot().document.offset;
    std::vector<runtime::ComponentId> controls;
    for (const auto& item : f.inputs.mounted_inputs()) {
        if (f.within(item.component, page)) {
            controls.push_back(item.component);
        }
    }
    f.host.set_locale(locales::en_US);
    f.sync();
    f.host.set_format_locale(locales::en_US);
    f.host.set_direction(TextDirection::RightToLeft);
    f.sync();
    require(f.host.snapshot().page == page && f.host.work().page_mounts == mounts &&
                f.host.sample_runs("gallery.sample.input.basics") == runs && editor.value() == draft &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && f.host.snapshot().document.offset == offset,
            "locale/format/direction update rebuilt a page or changed draft/selection/preedit/undo/scroll");
    for (const auto id : controls) {
        require(f.services.components().contains(id), "a retained page control was replaced");
    }
    f.viewport.width = 960;
    f.sync();
    require(f.host.snapshot().page == page && f.host.work().page_mounts == mounts,
            "localized page resize remounted the document");
    f.show("ant.component.select");
    require(!f.services.components().contains(page) && !f.inputs.editors().find(input.editor),
            "localized navigation retained an old page/editor");
    auto& selects = f.services.select();
    const auto owner = f.host.target("gallery.select.controlled-open");
    require(owner.valid(), "real controlled Select target is absent");
    const auto before = selects.snapshot(owner);
    // The registered action uses the real controlled open callback.
    const auto action = f.host.target("gallery.select.toggle-open");
    const auto lane = f.host.snapshot().layout.document;
    const auto& action_node = f.nodes.require(f.services.components().root(action));
    f.host.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2,
                            -(action_node.bounds.y + action_node.translation.y - lane.y - lane.height / 3) / 48);
    f.sync();
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (f.within(button.component, action)) {
            require(f.services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
                    "controlled Select action focus failed");
            f.host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
            f.host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
            break;
        }
    }
    f.sync();
    const auto opened = selects.snapshot(owner);
    require(opened.visible, "real Gallery Select did not open");
    const auto root = f.host.snapshot().page;
    f.host.set_locale(locales::zh_CN);
    f.sync();
    const auto switched = selects.snapshot(owner);
    require(switched.visible && switched.selector == before.selector && switched.value == opened.value &&
                f.host.snapshot().page == root,
            "language change closed or remounted a real Select popup");
    f.show("ant.component.button");
    f.host.set_locale(locales::en_US);
    f.sync();
    static_cast<void>(f.frames.consume_request());
    const auto requests = f.frames.counters().requests;
    f.host.set_locale(locales::en_US);
    f.host.set_messages(MessageCatalog(std::vector<MessageTranslation>(
        gallery_message_catalog().translations().begin(), gallery_message_catalog().translations().end())));
    require(!f.frames.pending() && requests == f.frames.counters().requests && !f.services.next_frame_deadline(),
            "equal locale/catalog added an idle frame or localization deadline");
    f.host.shutdown();
    static_cast<void>(f.frames.consume_request());
    f.host.set_locale(locales::zh_CN);
    require(!f.frames.pending(), "retired Gallery accepted a locale update");
}

void saved_destination_reprojects_without_rewriting_payload() {
    Fixture f;
    f.show("ant.component.tree-select");
    const auto note = f.editor("gallery.tree-select.note");
    f.replace(note, u8"delivery {literal} · 中文");
    f.press(f.host.target("gallery.tree-select.save"));
    require(f.has_fragment("已保存目标=guide / delivery {literal} · 中文"),
            "actual destination save did not capture its business payload");
    const auto page = f.host.snapshot().page;
    const auto mounts = f.host.work().page_mounts;
    const auto runs = f.host.sample_runs("gallery.sample.tree-select.destination");
    f.host.set_locale(locales::en_US);
    f.sync();
    require(f.has_fragment("Saved destination=guide / delivery {literal} · 中文") &&
                f.inputs.editors().require(note.editor).value() == "delivery {literal} · 中文" &&
                f.host.snapshot().page == page && f.host.work().page_mounts == mounts &&
                f.host.sample_runs("gallery.sample.tree-select.destination") == runs,
            "saved destination cached translated output, changed payload or reran its factory");
    f.host.set_locale(locales::zh_CN);
    f.sync();
    require(f.has_fragment("已保存目标=guide / delivery {literal} · 中文"),
            "saved message failed to restore Chinese while retaining its typed arguments");
}

void current_application_validation_reprojects_without_a_new_submit() {
    Fixture f;
    f.show("ant.component.form");
    const auto count = f.editor("gallery.form.count");
    f.replace(count, u8"0");
    f.host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
    f.host.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
    f.press(f.host.target("gallery.form.validate"));
    require(f.has(u8"数量至少为 1"), "actual Form custom validator did not expose its declared error");
    const auto page = f.host.snapshot().page;
    const auto runs = f.host.sample_runs("gallery.sample.form.fields");
    const auto value = std::string{f.inputs.editors().require(count.editor).value()};
    f.host.set_locale(locales::en_US);
    f.sync();
    require(f.has(u8"Quantity must be at least 1") && f.host.snapshot().page == page &&
                f.host.sample_runs("gallery.sample.form.fields") == runs &&
                f.inputs.editors().require(count.editor).value() == value,
            "current application Form issue failed to reproject or changed the real editor");
    f.host.set_locale(locales::zh_CN);
    f.sync();
    require(f.has(u8"数量至少为 1"), "current custom Form error lost its source on a repeated switch");
}

void new_table_filter_and_tour_step_keep_retained_identity() {
    Fixture f;
    f.show("ant.component.table");
    const auto target = f.host.target("gallery.table.virtual");
    runtime::ComponentId grid;
    for (const auto id : f.services.table().mounted()) {
        if (f.within(id, target)) {
            grid = id;
        }
    }
    require(grid.valid(), "new Gallery virtual Table is missing");
    const auto initial = f.services.table().snapshot(grid);
    require(initial.filter_buttons.size() == 3 && initial.filter_buttons.back().valid(),
            "new Gallery filter column has no actual trigger");
    f.press(initial.filter_buttons.back());
    const auto before = f.services.table().snapshot(grid);
    const auto work = f.services.table().work(grid);
    const auto page = f.host.snapshot().page;
    f.host.set_locale(locales::en_US);
    f.sync();
    const auto after = f.services.table().snapshot(grid);
    const auto after_work = f.services.table().work(grid);
    require(f.has(u8"Locked") && f.has(u8"Editable") && f.has(u8"Fixed end column / filter") &&
                before.rows.size() == after.rows.size() && before.filter_buttons == after.filter_buttons &&
                work.copied_records == after_work.copied_records && work.sort_passes == after_work.sort_passes &&
                f.host.snapshot().page == page,
            "Table filter/column copy did not update independently of data projection");
    for (std::size_t i = 0; i < before.rows.size(); ++i) {
        require(before.rows[i].component == after.rows[i].component && before.rows[i].cells == after.rows[i].cells,
                "Table language change replaced a retained row/cell");
    }
    f.show("ant.component.tour");
    f.press(f.host.target("gallery.tour.open"));
    const auto draft = f.editor("gallery.tour.draft");
    f.replace(draft, u8"Tour {payload} · 中文");
    const auto step = f.host.target("gallery.tour.draft");
    const auto runs = f.host.sample_runs("gallery.sample.tour.native");
    f.host.set_locale(locales::zh_CN);
    f.sync();
    require(f.has(u8"第一步：原生编辑目标") && f.host.target("gallery.tour.draft") == step &&
                f.inputs.editors().require(draft.editor).value() == "Tour {payload} · 中文" &&
                f.host.sample_runs("gallery.sample.tour.native") == runs,
            "Tour language change replaced the open rich step, draft or target");
}

} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        declared_keys_schemas_and_fallback();
        real_shell_click_and_missing_catalog();
        retained_page_editor_popup_and_retirement();
        saved_destination_reprojects_without_rewriting_payload();
        current_application_validation_reprojects_without_a_new_submit();
        new_table_filter_and_tour_step_keep_retained_identity();
        std::cout << "i18n Gallery: 6 real-host contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
