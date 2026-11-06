#include "component/app_component.hpp"
#include "component/notice_component.hpp"
#include "component/image_component.hpp"
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
    a.services.dispose();
    require(ah.closed().get() == NoticeCloseResult::Cancelled && br.bound() &&
                bh.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready,
            "window teardown settled another window's future");
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
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        live_context();
        independent_windows();
        motion_completion();
        std::cout << "Notice App live i18n/Theme, windows and late completion completed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
