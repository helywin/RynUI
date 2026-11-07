#include "component/app_component.hpp"
#include "component/notice_component.hpp"
#include "component/image_component.hpp"
#include "component/modal_component.hpp"
#include "component/popconfirm_component.hpp"
#include "support/input_fixture.hpp"
#include "support/i18n_test.hpp"

#include <chrono>
#include <thread>

namespace {
using namespace ryn;
using ryn_test::i18n::require;
using Fixture = ryn_test::input_component::Fixture;

const MessageKey title{u8"service.title", String{u8"Live notice"}};
const MessageKey action{u8"service.action", String{u8"Continue"}};

MessageCatalog dictionary() {
    return MessageCatalog({{locales::zh_CN, title, String{u8"实时通知"}}, {locales::zh_CN, action, String{u8"继续"}}});
}

void sync(Fixture& f) {
    require(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "i18n service layout failed");
}

runtime::ComponentId entry(Fixture& f, const String& key) {
    for (const auto holder : f.services.notices().holders()) {
        for (const auto& item : f.services.notices().entries(holder)) {
            if (item.key == key) {
                return item.id;
            }
        }
    }
    throw std::runtime_error("Missing actual service entry");
}

bool text_exists(Fixture& f, String text) {
    for (const auto& mounted : f.services.text().mounted_texts()) {
        if (f.scene.text_state(mounted.scene).content() == text.view()) {
            return true;
        }
    }
    return false;
}

bool panel_text(Fixture& f, runtime::ComponentId root, String text) {
    for (const auto& mounted : f.services.text().mounted_texts()) {
        if (f.scene.text_state(mounted.scene).content() != text.view()) {
            continue;
        }
        for (auto id = std::optional{mounted.component}; id; id = f.services.components().parent(*id)) {
            if (*id == root) {
                return true;
            }
        }
    }
    return false;
}

void live_context() {
    Fixture f;
    Signal<LocaleId> language{locales::zh_CN};
    auto theme = std::make_shared<Signal<ThemeConfig>>(ThemeConfig{});
    auto initial = std::make_unique<ThemeConfig>();
    initial->seed.color_primary = Color::rgba8(180, 30, 80);
    theme->set(*initial);
    AppRef explicit_ref;
    AppRef captured;
    AppRef from_service;
    I18n tr;
    InputRef ref;
    runtime::ComponentId source;
    runtime::CapturedComponentContext context;
    int slot_runs{};
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(explicit_ref), AppContent{[&] {
                Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                         ConfigProvider(ConfigProviderProps{}
                                            .locale(language)
                                            .messages(dictionary())
                                            .direction(TextDirection::RightToLeft)
                                            .theme(*theme),
                                        ConfigProviderContent{[&] {
                                            captured = useApp();
                                            tr = useI18n();
                                            context = runtime::require_component_build_context().capture_context();
                                            source = *context.component_owner;
                                        }});
                     }});
            }});
    }});
    const auto handle =
        captured.notification().openAsync(u8"live", NotificationProps{}.title(tr.text(title)).duration(0),
                                          {.description = NotificationDescription{[&] {
                                               ++slot_runs;
                                               from_service = useApp();
                                               const auto local = useI18n();
                                               Text(TextProps{}.content(local.text(action)));
                                               Input(InputProps{}.ref(ref).defaultValue(u8"a中文z"));
                                           }}});
    const auto literal = captured.message().openAsync(u8"literal", MessageProps{}.content(u8"Live notice").duration(0));
    sync(f);
    const auto id = entry(f, u8"live");
    const auto mounted = f.inputs.mounted_inputs().back();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(f.services.components().config_scope(id) == context.config &&
                f.services.components().theme_scope(id) == context.theme &&
                context.config->snapshot().direction == TextDirection::RightToLeft &&
                context.theme->snapshot().seed().color_primary == *initial->seed.color_primary,
            "service froze or inherited the holder's config/Theme instead of its caller");
    require(text_exists(f, u8"实时通知") && text_exists(f, u8"继续") && text_exists(f, u8"Live notice") &&
                from_service.bound(),
            "real service content lost its App/i18n context or literal text");
    require(ref.focus() && ref.select(7, 1), "service reverse selection setup failed");
    const auto selection = editor.selection();
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "service preedit setup failed");
    const auto history = editor.history();
    const auto draft = std::string(editor.value());
    language.set(locales::en_US);
    auto changed = std::make_unique<ThemeConfig>();
    changed->seed.color_primary = Color::rgba8(20, 130, 190);
    theme->set(*changed);
    require(handle.update(NotificationProps{}.status(NoticeStatus::Success)), "live-context update failed");
    sync(f);
    require(text_exists(f, u8"Continue") && !text_exists(f, u8"继续") && text_exists(f, u8"Live notice") &&
                slot_runs == 1 && entry(f, u8"live") == id &&
                f.inputs.mounted_inputs().back().editor == mounted.editor && editor.value() == draft &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && f.inputs.sessions().active() == stamp &&
                context.theme->snapshot().seed().color_primary == *changed->seed.color_primary,
            "language/Theme update reset service identity, draft, selection, preedit, history or session");
    bool mismatch{};
    try {
        static_cast<void>(explicit_ref.notification().open(u8"live", NotificationProps{}.title(u8"Wrong context")));
    } catch (const std::invalid_argument&) {
        mismatch = true;
    }
    require(mismatch && entry(f, u8"live") == id && !text_exists(f, u8"Wrong context"),
            "same-key context mismatch partially applied props");
    require(from_service.message().open(u8"nested", MessageProps{}.content(u8"Nested service").duration(0)),
            "service content could not use its nearest real App outside mount");
    require(f.services.destroy(source) && !captured.bound() && !from_service.bound() && explicit_ref.bound() &&
                handle.closed().get() == NoticeCloseResult::Cancelled &&
                literal.closed().get() == NoticeCloseResult::Cancelled && !ref.bound() &&
                !f.inputs.sessions().active().valid(),
            "captured Provider/source retirement did not cancel just its own service entries");
    require(!captured.message().open(u8"late", MessageProps{}) &&
                !handle.update(NotificationProps{}.title(u8"Late completion")),
            "late service completion reached a retired captured context");
    sync(f);
    require(!f.services.next_frame_deadline(), "retired service retained an animation/text deadline");
}

void independent_windows() {
    Fixture a;
    Fixture b;
    AppRef ar;
    AppRef br;
    I18n at;
    I18n bt;
    Signal<LocaleId> language{locales::zh_CN};
    a.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).messages(dictionary()),
                       ConfigProviderContent{[&] { App(AppProps{}.ref(ar), AppContent{[&] { at = useI18n(); }}); }});
    }});
    b.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(locales::en_US).messages(dictionary()),
                       ConfigProviderContent{[&] { App(AppProps{}.ref(br), AppContent{[&] { bt = useI18n(); }}); }});
    }});
    const auto ah = ar.message().openAsync(u8"same", MessageProps{}.content(at.text(title)).duration(0));
    const auto bh = br.message().openAsync(u8"same", MessageProps{}.content(bt.text(title)).duration(0));
    sync(a);
    sync(b);
    require(text_exists(a, u8"实时通知") && text_exists(b, u8"Live notice"), "window locale leaked between services");
    language.set(locales::en_US);
    require(ah.update(MessageProps{}.status(NoticeStatus::Success)), "late-context update failed");
    sync(a);
    sync(b);
    require(text_exists(a, u8"Live notice") && text_exists(b, u8"Live notice") &&
                ah.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready,
            "config revision invalidated a valid request/future");
    const auto modal =
        ar.modal().confirm(ModalProps{}.title(at.text(title)), ModalContent{[] { Input(InputProps{}); }});
    const auto drawer =
        br.drawer().open(DrawerProps{}.title(bt.text(title)), DrawerContent{[] { Input(InputProps{}); }});
    const auto modal_root = a.services.modal().mounted().back();
    const auto drawer_root = b.services.modal().mounted().back();
    language.set(locales::zh_CN);
    sync(a);
    sync(b);
    require(panel_text(a, modal_root, u8"实时通知") && panel_text(a, modal_root, u8"确定") &&
                panel_text(b, drawer_root, u8"Live notice") && !panel_text(b, drawer_root, u8"实时通知"),
            "panel Provider update crossed windows");
    a.services.dispose();
    require(ah.closed().get() == NoticeCloseResult::Cancelled && br.bound() &&
                bh.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready &&
                modal.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready &&
                modal.closed().get().kind == PanelCloseKind::OwnerDisposed && drawer.bound(),
            "window teardown settled another window's future");
    require(drawer.destroy() && drawer.closed().get().kind == PanelCloseKind::Destroyed,
            "other window Drawer cleanup failed");
    require(bh.close() && bh.closed().get() == NoticeCloseResult::Closed && !bh.close(),
            "remaining window future did not resolve exactly once");
}

void motion_completion() {
    Fixture f;
    AppRef app;
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[] {}}); }});
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.services.set_animation_time(animation::AnimationTime::microseconds(0));
    const auto first = app.message().openAsync(u8"motion", MessageProps{}.content(u8"First").duration(0).motion(true));
    sync(f);
    require(first.close(), "motion close request failed");
    const auto reopened = app.message().openAsync(u8"motion", MessageProps{}.content(u8"Reopened").duration(0));
    f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    sync(f);
    require(text_exists(f, u8"Reopened") &&
                first.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready &&
                reopened.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready,
            "old exit completion closed a reopened entry");
    require(reopened.close(), "reopened motion close failed");
    f.services.tick_animations(animation::AnimationTime::microseconds(2000000));
    require(first.closed().get() == NoticeCloseResult::Closed && reopened.closed().get() == NoticeCloseResult::Closed,
            "accepted futures did not settle at the actual exit boundary");
    f.services.tick_animations(animation::AnimationTime::microseconds(3000000));
    require(first.closed().get() == NoticeCloseResult::Closed && !first.close() && !f.services.next_frame_deadline(),
            "late exit/timer repeated settlement or retained a deadline");
}

void panel_context(int kind) {
    Fixture f;
    AppRef app;
    AppRef captured;
    AppRef from_service;
    PopconfirmRef anchor;
    Signal<LocaleId> language{locales::zh_CN};
    const auto theme = std::make_shared<Signal<ThemeConfig>>(ThemeConfig{});
    runtime::CapturedComponentContext context;
    I18n tr;
    InputRef ref;
    int mounts{};
    int cancels{};
    int actions{};
    ConfirmAction::Complete done;
    std::vector<std::function<void()>> posted;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         ConfigProvider(ConfigProviderProps{}.locale(language).messages(dictionary()).theme(*theme),
                                        ConfigProviderContent{[&] {
                                            captured = useApp();
                                            tr = useI18n();
                                            context = runtime::require_component_build_context().capture_context();
                                            Popconfirm(PopconfirmProps{}.ref(anchor), PopconfirmTrigger{[] {
                                                           Button(ButtonProps{}, [] { Text(u8"Anchor"); });
                                                       }});
                                        }});
                     }});
            }});
    }});
    const auto content = [&] {
        ++mounts;
        from_service = useApp();
        Text(TextProps{}.content(useI18n().text(action)));
        Input(InputProps{}.ref(ref).defaultValue(u8"a中文z"));
    };
    const ConfirmAction source{[&](ConfirmActionRequest, ConfirmAction::Complete complete) {
                                   ++actions;
                                   done = std::move(complete);
                                   return [&] {
                                       ++cancels;
                                   };
                               },
                               [&](std::function<void()> fn) { posted.push_back(std::move(fn)); }};
    ModalHandle modal;
    DrawerHandle drawer;
    PopconfirmHandle pop;
    std::shared_future<PanelCloseResult> future;
    runtime::ComponentId root;
    runtime::ComponentId confirm;
    if (kind == 0) {
        modal = captured.modal().confirm(ModalProps{}.title(tr.text(title)).onOkAsync(source), ModalContent{content});
        root = f.services.modal().mounted().back();
        confirm = f.services.modal().snapshot(root).ok;
        future = modal.closed();
    } else if (kind == 1) {
        drawer = captured.drawer().open(DrawerProps{}.title(tr.text(title)), DrawerContent{content});
        root = f.services.modal().mounted().back();
        future = drawer.closed();
    } else {
        pop = captured.popconfirm().confirm(anchor, PopconfirmProps{}.title(tr.text(title)).onConfirmAsync(source),
                                            {.description = PopconfirmDescription{content}});
        root = f.services.popconfirm().mounted().back();
        confirm = f.services.popconfirm().snapshot(root).confirm;
        future = pop.closed();
    }
    sync(f);
    require(from_service.bound() && mounts == 1 && f.services.components().config_scope(root) == context.config &&
                panel_text(f, root, u8"实时通知") && panel_text(f, root, u8"继续") &&
                (kind == 1 || (panel_text(f, root, u8"确定") && panel_text(f, root, u8"取消"))),
            "panel lost captured Provider/App/default footer");
    if (confirm.valid()) {
        const auto& node = f.nodes.require(f.services.components().root(confirm));
        const float x = node.bounds.x + node.translation.x + node.bounds.width / 2;
        const float y = node.bounds.y + node.translation.y + node.bounds.height / 2;
        for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
            f.services.pointer().dispatch(
                {input::PointerIdentity::mouse(), action, input::PointerButton::primary, x, y});
        }
        if (actions != 1) {
            std::cerr << "panel action kind=" << kind << " point=" << x << ',' << y << " confirm=" << confirm.index;
            const auto pointer = f.services.pointer().state(input::PointerIdentity::mouse());
            if (pointer && pointer->hover_target) {
                const auto* hit = f.services.interactions().find(*pointer->hover_target);
                if (hit) {
                    std::cerr << " hit=" << hit->component.index;
                }
            }
            std::cerr << '\n';
        }
        require(actions == 1, "actual panel footer did not start business action");
    }
    const auto mounted = f.inputs.mounted_inputs().back();
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(ref.focus() && ref.select(7, 1), "panel editor selection setup failed");
    const auto selection = editor.selection();
    const auto stamp = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"拼"}, {0, 1}, stamp})),
            "panel preedit setup failed");
    const auto history = editor.history();
    const auto draft = std::string(editor.value());
    language.set(locales::en_US);
    const auto next_theme = std::make_unique<ThemeConfig>();
    next_theme->seed.color_primary = Color::rgba8(20, 130, 190);
    theme->set(*next_theme);
    if (kind == 0) {
        require(modal.update({.cancel_text = Prop<String>{String{u8"Literal"}}}), "Modal update failed");
    }
    if (kind == 1) {
        require(drawer.update({.title = tr.text(title)}), "Drawer update failed");
    }
    if (kind == 2) {
        require(pop.update({.cancel_text = Prop<String>{String{u8"Literal"}}}), "Popconfirm update failed");
    }
    sync(f);
    require(mounts == 1 && f.services.components().theme_scope(root) == context.theme &&
                context.theme->snapshot().seed().color_primary == *next_theme->seed.color_primary &&
                panel_text(f, root, u8"Live notice") && panel_text(f, root, u8"Continue") &&
                (kind == 1 || (panel_text(f, root, u8"Literal") && panel_text(f, root, u8"OK"))) && cancels == 0 &&
                editor.value() == draft && editor.selection() == selection && editor.composition().active &&
                editor.history().undo_count == history.undo_count && f.inputs.sessions().active() == stamp &&
                future.wait_for(std::chrono::seconds(0)) != std::future_status::ready,
            "locale/update cancelled business action or lost editor identity/composition");
    if (confirm.valid()) {
        std::thread worker([&] {
            done({ConfirmActionDecision::KeepOpen});
            done({});
        });
        worker.join();
        require(posted.size() == 1, "duplicate worker result posted twice");
        posted.front()();
        posted.clear();
        require(future.wait_for(std::chrono::seconds(0)) != std::future_status::ready && cancels == 0,
                "locale revision invalidated action or KeepOpen closed future");
    }
    require(f.services.destroy(*context.component_owner) && !captured.bound() && app.bound() && !ref.bound() &&
                future.wait_for(std::chrono::seconds(0)) == std::future_status::ready &&
                future.get().kind == PanelCloseKind::OwnerDisposed,
            "panel source retirement did not settle once");
    sync(f);
    require(!f.services.next_frame_deadline() && !f.inputs.sessions().active().valid(),
            "panel retained deadline/text route");
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        live_context();
        independent_windows();
        motion_completion();
        for (int kind = 0; kind < 3; ++kind) {
            panel_context(kind);
        }
        std::cout << "Notice App live i18n/Theme, windows and late completion completed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
