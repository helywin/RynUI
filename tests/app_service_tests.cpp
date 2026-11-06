#include "component/app_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#include "component/notice_component.hpp"
#include "component/image_component.hpp"
#include "support/input_fixture.hpp"
#include "support/i18n_test.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string_view>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::input;
using ryn_test::i18n::require;
using Fixture = ryn_test::input_component::Fixture;

void sync(Fixture& f) {
    require(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "App layout failed");
}

runtime::ComponentId owner() {
    return runtime::require_component_build_context().nearest_state<detail::AppState>()->binding->owner;
}

runtime::ComponentId entry(Fixture& f, const String& key) {
    for (const auto holder : f.services.notices().holders()) {
        for (const auto& item : f.services.notices().entries(holder)) {
            if (item.key == key) {
                return item.id;
            }
        }
    }
    return {};
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    const runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                               node.bounds.y + node.translation.y + node.bounds.height / 2};
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, point.x, point.y});
    }
    sync(f);
}

bool ready(const NoticeHandle& handle) {
    return handle.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <typename Function> void rejected(Function&& fn) {
    bool failed{};
    try {
        fn();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, "invalid App service request was accepted");
}

void binding() {
    rejected([] { static_cast<void>(useApp()); });
    AppRef app;
    require(!app.bound() && !app.message().bound(), "unmounted App is bound");
    const auto cancelled = app.message().openAsync(u8"missing", MessageProps{});
    require(ready(cancelled) && cancelled.closed().get() == NoticeCloseResult::Cancelled,
            "unbound request did not settle Cancelled");
    Fixture f;
    runtime::ComponentId root;
    runtime::ComponentId launch;
    runtime::ComponentId action;
    InputRef editor;
    NoticeHandle handle;
    int actions{};
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                root = owner();
                const auto captured = useApp();
                Button(ButtonProps{}.onClick([&, captured] {
                    handle = captured.notification().openAsync(
                        u8"event", NotificationProps{}.title(u8"Event service").duration(0),
                        {.description =
                             NotificationDescription{[&] { Input(InputProps{}.ref(editor).defaultValue(u8"draft")); }},
                         .actions = NotificationActions{[&] {
                             Button(ButtonProps{}.onClick([&] { ++actions; }), [] { Text(u8"Action"); });
                             action = f.buttons.mounted_buttons().back().component;
                         }}});
                }),
                       [] { Text(u8"Open service"); });
                launch = f.buttons.mounted_buttons().back().component;
            }});
    }});
    sync(f);
    require(app.bound() && f.services.notices().holders().size() == 2 && !editor.bound(),
            "App did not mount exactly its real automatic holders");
    click(f, launch);
    require(editor.bound() && editor.focus() && !ready(handle), "event did not mount a real interactive Input");
    click(f, action);
    require(actions == 1, "service action did not receive real pointer input");
    require(handle.close() && ready(handle) && handle.closed().get() == NoticeCloseResult::Closed,
            "service did not settle after close");
    require(f.services.destroy(root) && !app.bound() && !editor.bound(), "App teardown retained public refs");
}

void nested() {
    Fixture f;
    AppRef outer;
    AppRef inner;
    runtime::ComponentId outer_root;
    runtime::ComponentId inner_root;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(outer), AppContent{[&] {
                outer_root = owner();
                App(AppProps{}.ref(inner), AppContent{[&] {
                        inner_root = owner();
                        const auto nearest = useApp();
                        require(nearest.bound(), "nested App hook is unbound");
                    }});
            }});
    }});
    const auto a = outer.message().openAsync(u8"same", MessageProps{}.content(u8"Outer").duration(0));
    const auto b = inner.message().openAsync(u8"same", MessageProps{}.content(u8"Inner").duration(0));
    require(f.services.destroy(inner_root) && ready(b) && b.closed().get() == NoticeCloseResult::Cancelled &&
                !ready(a) && outer.bound() && !inner.bound(),
            "nested App teardown closed another App's entry");
    auto stale = outer;
    const auto old_service = stale.message();
    require(f.services.destroy(outer_root) && ready(a), "outer App did not cancel its own entry");
    runtime::ComponentId anchor;
    // The original explicit ref can bind a new generation. Copies captured
    // after the previous bind and service refs must stay retired.
    Fixture rebound;
    rebound.services.mount(Content{[&] { App(AppProps{}.ref(outer), AppContent{[&] { anchor = owner(); }}); }});
    require(outer.bound() && !stale.bound() && !old_service.open(u8"late", MessageProps{}),
            "old App copy revived when the original ref was rebound");
    require(rebound.services.destroy(anchor), "rebound App cleanup failed");
    Fixture duplicate;
    rejected([&] {
        duplicate.services.mount(Content{
            [&] { App(AppProps{}.ref(inner), AppContent{[&] { App(AppProps{}.ref(inner), AppContent{[] {}}); }}); }});
    });
    require(!inner.bound() && duplicate.services.components().component_count() == 0,
            "duplicate live binding did not unwind its owner");
}

void windows() {
    Fixture a;
    Fixture b;
    AppRef first;
    AppRef second;
    runtime::ComponentId ar;
    runtime::ComponentId br;
    runtime::ComponentId button;
    runtime::CapturedComponentContext context;
    a.services.mount(Content{[&] {
        App(AppProps{}.ref(first), AppContent{[&] {
                ar = owner();
                context = runtime::require_component_build_context().capture_context();
            }});
    }});
    b.services.mount(Content{[&] {
        App(AppProps{}.ref(second), AppContent{[&] {
                br = owner();
                Button(ButtonProps{}.onClick([&] {
                    require(first.message().open(u8"explicit-target", MessageProps{}.duration(0)),
                            "an explicit ref lost its own target window");
                }),
                       [] { Text(u8"Call the first window"); });
                button = b.buttons.mounted_buttons().back().component;
            }});
    }});
    sync(a);
    sync(b);
    click(b, button);
    require(entry(a, u8"explicit-target").valid() && !entry(b, u8"explicit-target").valid(),
            "service selected the callback's window instead of its explicit owner");
    const auto count = b.services.components().component_count();
    rejected([&] { b.services.append_slot(br, Content{[] { Text(u8"foreign"); }}, context); });
    require(b.services.components().component_count() == count && second.bound(),
            "foreign captured context changed the target window");
    bool wrong_thread_bound{true};
    NoticeCloseResult wrong_thread_result{NoticeCloseResult::Failed};
    std::thread worker([&] {
        wrong_thread_bound = first.bound();
        wrong_thread_result = first.message().openAsync(u8"worker", MessageProps{}).closed().get();
    });
    worker.join();
    require(!wrong_thread_bound && wrong_thread_result == NoticeCloseResult::Cancelled && !entry(a, u8"worker").valid(),
            "foreign thread entered the window host");
    require(a.services.destroy(ar) && second.bound() && b.services.destroy(br), "window cleanup crossed owners");
}

void update() {
    Fixture f;
    AppRef app;
    InputRef ref;
    runtime::ComponentId root;
    int mounts{};
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    const auto service = app.notification();
    const auto handle = service.openAsync(u8"retained", NotificationProps{}.title(u8"Before").duration(0),
                                          {.description = NotificationDescription{[&] {
                                               ++mounts;
                                               Input(InputProps{}.ref(ref).defaultValue(u8"a中文z"));
                                           }}});
    sync(f);
    const auto id = entry(f, u8"retained");
    const auto input = f.inputs.mounted_inputs().back();
    auto& editor = f.inputs.editors().require(input.editor);
    require(ref.focus() && ref.select(7, 1), "retained reverse selection setup failed");
    const auto selection = editor.selection();
    require(bool(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "retained composition setup failed");
    const auto history = editor.history();
    require(handle.update(NotificationProps{}.title(u8"Updated").status(NoticeStatus::Success)) &&
                service.open(u8"retained", NotificationProps{}.placement(NotificationPlacement::BottomRight)),
            "same context update failed");
    sync(f);
    require(entry(f, u8"retained") == id && mounts == 1 && f.inputs.mounted_inputs().back().editor == input.editor &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && !ready(handle),
            "Notice update rebuilt its retained editor or settled its future");
    const auto previous = f.services.notices().snapshot(id).description;
    rejected([&] {
        static_cast<void>(service.openAsync(
            u8"retained", NotificationProps{},
            {.description = NotificationDescription{[] { throw std::runtime_error("candidate failure"); }}}));
    });
    require(f.services.components().contains(previous) && f.services.notices().snapshot(id).description == previous &&
                ref.bound() && !ready(handle),
            "throwing candidate replaced the old region or failed its old completion");
    require(handle.close() && handle.closed().get() == NoticeCloseResult::Closed && !ref.bound(), "close failed");
    const auto fresh = service.openAsync(u8"retained", NotificationProps{}.duration(0));
    require(!handle.close() && !handle.update(NotificationProps{}.title(u8"late")) && !ready(fresh),
            "a retired handle reached a new entry with the same key");
    require(f.services.destroy(root) && fresh.closed().get() == NoticeCloseResult::Cancelled,
            "new entry did not settle on owner disposal");
}

void callbacks() {
    Fixture f;
    AppRef app;
    runtime::ComponentId root;
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    const auto service = app.message();
    NoticeHandle reopened;
    int closes{};
    const auto old = service.openAsync(u8"key", MessageProps{}.duration(0).onClose([&] {
        ++closes;
        reopened = service.openAsync(u8"key", MessageProps{}.content(u8"New").duration(0));
    }));
    require(old.close() && closes == 1 && old.closed().get() == NoticeCloseResult::Closed && !ready(reopened),
            "close callback reopened an entry that old cleanup then retired");
    require(!old.close() && !old.update(MessageProps{}.content(u8"late")), "old completion revived");
    require(reopened.close(), "reopened close failed");
    int callbacks{};
    const auto first = service.openAsync(u8"throw", MessageProps{}.duration(0).onClose([&] {
        ++callbacks;
        throw std::runtime_error("onClose failure");
    }));
    const auto second = service.openAsync(u8"after", MessageProps{}.duration(0).onClose([&] { ++callbacks; }));
    rejected([&] { static_cast<void>(service.clear()); });
    require(callbacks == 2 && first.closed().get() == NoticeCloseResult::Closed &&
                second.closed().get() == NoticeCloseResult::Closed && !entry(f, u8"after").valid(),
            "throwing close callback interrupted the remaining cleanup");
    const auto dying = service.openAsync(u8"destroy", MessageProps{}.duration(0).onClose([&] {
        require(f.services.destroy(root), "onClose owner destroy failed");
    }));
    require(dying.close() && dying.closed().get() == NoticeCloseResult::Closed && !app.bound(),
            "close callback owner destruction accessed retired state");
}

void failures() {
    for (const bool throws : {false, true}) {
        Fixture f;
        AppRef app;
        runtime::ComponentId root;
        f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
        const auto service = app.notification();
        auto request = [&] {
            return service.openAsync(u8"retire", NotificationProps{}.duration(0),
                                     {.description = NotificationDescription{[&] {
                                          require(f.services.destroy(root), "slot owner retirement failed");
                                          if (throws) {
                                              throw std::runtime_error("retired slot failure");
                                          }
                                      }}});
        };
        if (throws) {
            rejected([&] { static_cast<void>(request()); });
        } else {
            const auto handle = request();
            require(ready(handle) && handle.closed().get() == NoticeCloseResult::Cancelled,
                    "retired mount left a pending/broken future");
        }
        require(!app.bound() && f.services.components().component_count() == 0 &&
                    f.services.interactions().size() == 0 && f.inputs.editors().size() == 0,
                "retired slot retained components/input resources");
    }
    Fixture getter;
    AppRef app;
    runtime::ComponentId root;
    getter.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    const auto handle = app.message().openAsync(u8"getter", MessageProps{}.content(bind([&] {
        getter.services.destroy(root);
        return String{u8"late getter"};
    })));
    require(ready(handle) && handle.closed().get() == NoticeCloseResult::Cancelled && !app.bound(),
            "props getter retirement continued applying an old request");
    Fixture scope;
    AppRef scoped;
    runtime::ComponentId scoped_root;
    InputRef editor;
    scope.services.mount(Content{[&] { App(AppProps{}.ref(scoped), AppContent{[&] { scoped_root = owner(); }}); }});
    const auto pending = scoped.notification().openAsync(
        u8"scope", NotificationProps{}.duration(0),
        {.description = NotificationDescription{[&] { Input(InputProps{}.ref(editor)); }}});
    sync(scope);
    require(editor.focus(), "scope retirement focus setup failed");
    scope.services.components().scope(scoped_root).dispose();
    require(!scoped.bound() && !editor.bound() && scope.inputs.mounted_inputs().empty() &&
                scope.inputs.editors().size() == 0 && pending.closed().get() == NoticeCloseResult::Cancelled &&
                !scope.services.next_frame_deadline(),
            "Scope retirement kept an accepted future/editor alive");
}

void cleanup() {
    Fixture f;
    AppRef a;
    AppRef b;
    runtime::ComponentId ar;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(a), AppContent{[&] { ar = owner(); }});
        App(AppProps{}.ref(b), AppContent{[] {}});
    }});
    sync(f);
    const auto components = f.services.components().component_count();
    const auto nodes = f.nodes.size();
    const auto texts = f.scene.size();
    const auto effects = f.services.rounded_effects().live_count();
    const auto other = b.notification().openAsync(u8"other", NotificationProps{}.duration(0));
    for (int i = 0; i < 40; ++i) {
        InputRef editor;
        const auto handle = a.notification().openAsync(
            u8"loop", NotificationProps{}.duration(0),
            {.description = NotificationDescription{[&] { Input(InputProps{}.ref(editor)); }}});
        sync(f);
        require(editor.focus(), "cleanup editor focus failed");
        const auto stamp = f.inputs.sessions().active();
        require(a.notification().clear() && handle.closed().get() == NoticeCloseResult::Closed && !editor.bound() &&
                    !f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp}),
                "cleanup left a late text route");
    }
    require(!ready(other) && a.bound() && b.bound(), "loop cleanup touched another App");
    require(other.close(), "other App cleanup failed");
    sync(f);
    require(f.services.components().component_count() == components && f.nodes.size() == nodes &&
                f.scene.size() == texts && f.services.rounded_effects().live_count() == effects &&
                f.services.interactions().size() == 0 && f.inputs.editors().size() == 0 &&
                !f.inputs.sessions().active().valid() && !f.services.next_frame_deadline(),
            "App service resources/deadlines did not return to idle baseline");
    const auto handle = a.message().openAsync(u8"dispose", MessageProps{}.duration(0));
    require(f.services.destroy(ar) && handle.closed().get() == NoticeCloseResult::Cancelled && b.bound(),
            "App owner retirement crossed a sibling boundary");
    f.services.dispose();
    require(!b.bound() && f.services.components().component_count() == 0 && !f.services.next_frame_deadline(),
            "window disposal left App state alive");
}

void domain() {
    Fixture f;
    Signal<bool> modal{true};
    Signal<bool> spin{false};
    AppRef app;
    AppRef in_modal;
    InputRef background;
    InputRef editor;
    runtime::ComponentId action;
    int actions{};
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Input(InputProps{}.ref(background));
                Modal(ModalProps{}.open(modal), ModalContent{[&] { in_modal = useApp(); }});
            }});
        Spin(SpinProps{}.spinning(spin).fullscreen(true));
    }});
    sync(f);
    const auto handle = in_modal.notification().openAsync(
        u8"modal-notice", NotificationProps{}.duration(0),
        {.description = NotificationDescription{[&] { Input(InputProps{}.ref(editor)); }},
         .actions = NotificationActions{[&] {
             Button(ButtonProps{}.onClick([&] { ++actions; }), [] { Text(u8"Allowed action"); });
             action = f.buttons.mounted_buttons().back().component;
         }}});
    sync(f);
    require(editor.focus() && !background.focus() && f.services.components().in_window_layer_stack(action),
            "captured Modal Notice did not inherit its real input/paint domain");
    click(f, action);
    require(actions == 1, "Modal Notice action could not receive input");
    spin.set(true);
    sync(f);
    require(!editor.focus(), "Notice bypassed the fullscreen Spin winner");
    click(f, action);
    require(actions == 1, "Notice action escaped fullscreen input confinement");
    spin.set(false);
    sync(f);
    require(editor.focus(), "Notice did not return to the Modal input domain");
    require(handle.close(), "Modal Notice close failed");
    const auto timed = in_modal.message().openAsync(u8"hidden", MessageProps{}.content(u8"Timed").duration(5));
    modal.set(false);
    sync(f);
    require(background.focus() && !f.services.notices().next_auxiliary_deadline(),
            "hidden captured source retained service confinement or timers");
    require(timed.close(), "hidden source explicit close failed");
}

void clip() {
    Fixture f;
    AppRef app;
    const std::array bytes{std::byte{200}, std::byte{70}, std::byte{120}, std::byte{255}};
    const auto pixels = ImagePixels::copy(1, 1, 4, bytes);
    ImageRef image;
    runtime::ComponentId action;
    int actions{};
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[] {}}); }});
    const auto handle = app.notification().openAsync(
        u8"clip", NotificationProps{}.title(u8"Clipped rich content").duration(0),
        {.description = NotificationDescription{[&] {
             Flex(FlexProps{}.vertical(true).layout(LayoutStyle{}.height(dp(900)).flex_shrink(0)), FlexContent{[&] {
                      Image(
                          ImageProps{}
                              .ref(image)
                              .source(ImageSource{pixels})
                              .preview(false)
                              .fit(ImageFit::Fill)
                              .layout(LayoutStyle{}.width(dp(180)).height(dp(800)).flex_basis(dp(800)).flex_shrink(0)));
                      Button(
                          ButtonProps{}.onClick([&] { ++actions; }).layout(LayoutStyle{}.height(dp(40)).flex_shrink(0)),
                          [] { Text(u8"Outside clip"); });
                      action = f.buttons.mounted_buttons().back().component;
                  }});
         }}});
    sync(f);
    const auto id = entry(f, u8"clip");
    const auto region = f.services.notices().snapshot(id).description;
    const auto r = f.nodes.require(f.services.components().root(region)).bounds;
    bool published{};
    for (const auto& slot : f.services.images().slots()) {
        if (slot.live && slot.instance.pixels == pixels) {
            published = true;
            const auto actual = slot.instance.clip;
            require(actual.x >= r.x && actual.y >= r.y && actual.x + actual.width <= r.x + r.width + .01F &&
                        actual.y + actual.height <= r.y + r.height + .01F &&
                        slot.instance.bounds.height > actual.height,
                    "service image publication escaped its actual rich-content clip");
        }
    }
    require(published && image.bound(), "clipped rich content did not publish a real image");
    click(f, action);
    require(actions == 0 && handle.close() && !image.bound() && f.services.images().live_count() == 0,
            "overflow service action received input or image cleanup leaked");
}
} // namespace

int main(int argc, char** argv) {
    ryn_test::i18n::suppress_dialogs();
    try {
        const std::string_view name = argc > 1 ? argv[1] : "binding";
        if (name == "binding") {
            binding();
        } else if (name == "nested") {
            nested();
        } else if (name == "windows") {
            windows();
        } else if (name == "update") {
            update();
        } else if (name == "callbacks") {
            callbacks();
        } else if (name == "failures") {
            failures();
        } else if (name == "cleanup") {
            cleanup();
        } else if (name == "domain") {
            domain();
        } else if (name == "clip") {
            clip();
        } else {
            throw std::invalid_argument("Unknown App service case");
        }
        std::cout << "App service " << name << " completed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
