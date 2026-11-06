#include "component/qrcode_component.hpp"
#include "component/modal_component.hpp"
#include "support/timeline_fixture.hpp"
#include <fstream>
#include <thread>
#include <limits>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool v, const char* text) {
    ++checks;
    check(v, text);
}

void clean(Fixture& f) {
    f.services.dispose();
    expect(f.services.images().live_count() == 0 && f.services.interactions().size() == 0 &&
               f.services.surfaces().size() == 0 && blocked_roots(f.services.interactions()).empty(),
           "QRCode leaked retained resources/input blocks");
}

void basic() {
    Fixture f;
    Signal<QRCodeValue> value{QRCodeValue{u8"HELLO WORLD"}};
    Signal<QRCodeStatus> status{QRCodeStatus::Active};
    Signal<bool> disabled{false};
    QRCodeRef ref;
    int roots{};
    int refreshes{};
    f.services.mount(Content{[&] {
        ++roots;
        QRCode(QRCodeProps{}.value(value).status(status).disabled(disabled).ref(ref).onRefresh([&] { ++refreshes; }));
    }});
    sync(f);
    auto id = f.services.qrcode().mounted().front();
    auto v = f.services.qrcode().snapshot(id);
    expect(ref.bound() && v.data && v.data->size() == 21 && !v.modules.empty(), "QRCode has no actual encoded scene");
    expect(v.symbol.x > v.bounds.x && v.symbol.width < v.bounds.width, "QRCode omitted border padding/quiet zone");
    expect(!ref.refresh(), "Active code incorrectly refreshed");
    status.set(QRCodeStatus::Expired);
    sync(f);
    v = f.services.qrcode().snapshot(id);
    click(f, v.refresh);
    expect(refreshes == 1 && v.status == QRCodeStatus::Expired, "Refresh callback mutated controlled status");
    disabled.set(true);
    sync(f);
    expect(!ref.refresh(), "Disabled code refreshed");
    disabled.set(false);
    status.set(QRCodeStatus::Active);
    value.set(QRCodeValue{u8"New UTF-8 原生内容"});
    sync(f);
    expect(roots == 1 && ref.data()->modules().size() > 0, "Reactive content reran unrelated component");
    const auto png = ref.exportPng(400);
    const auto decoded = decode_image(*EncodedImage::copy(png));
    expect(bool(decoded) && decoded.pixels->width() == 400, "Ref did not export real PNG");
    expect(ref.exportSvg().find("<path") != std::string::npos, "Ref SVG is a placeholder");
    clean(f);
    expect(!ref.bound() && !ref.data() && !ref.refresh(), "Retired QR ref remained live");
    bool threw{};
    try {
        (void)ref.exportSvg();
    } catch (const std::logic_error&) {
        threw = true;
    }
    expect(threw, "Retired QR exported stale data");
}

void geometry() {
    for (float size : {0.F, .25F, 3.F, 37.5F, 160.25F}) {
        Fixture f;
        f.services.mount(
            Content{[&] { QRCode(QRCodeProps{}.value(u8"GEOMETRY").size(size).bordered(false).marginSize(4)); }});
        sync(f);
        const auto id = f.services.qrcode().mounted().front();
        const auto v = f.services.qrcode().snapshot(id);
        expect(std::isfinite(v.symbol.width), "QR geometry nonfinite");
        for (const auto& q : v.modules) {
            expect(q.opacity == 1 && q.bounds[2] >= 0 && q.bounds[3] >= 0 && std::isfinite(q.bounds[0]) &&
                       inside({q.bounds[0], q.bounds[1], q.bounds[2], q.bounds[3]}, v.symbol),
                   "QR module outside symbol or nonfinite");
        }
        if (size == 0) {
            expect(v.modules.empty(), "Zero code rendered modules");
        }
        clean(f);
    }
    Fixture f;
    Signal<unsigned> margin{4};
    Signal<float> size{160};
    Signal<QRCodeErrorLevel> level{QRCodeErrorLevel::M};
    f.services.mount(Content{[&] {
        QRCode(QRCodeProps{}.value(u8"LEVEL").marginSize(margin).size(size).errorLevel(level).boostLevel(false));
    }});
    sync(f);
    auto id = f.services.qrcode().mounted().front();
    const auto old = f.services.qrcode().snapshot(id).symbol;
    margin.set(0);
    level.set(QRCodeErrorLevel::H);
    size.set(80);
    sync(f);
    const auto v = f.services.qrcode().snapshot(id);
    expect(v.symbol.width < old.width && v.data->error_level() == QRCodeErrorLevel::H, "Reactive geometry/EC stale");
    clean(f);
}

void async() {
    Fixture f;
    std::vector<ImageCompletion> pending;
    int cancels{};
    int loaded{};
    const std::array<std::byte, 4> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{128}};
    auto pixels = ImagePixels::copy(1, 1, 4, red);
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion c) {
        pending.push_back(c);
        return [&] {
            ++cancels;
        };
    };
    Signal<ImageSource> logo{ImageSource{loader}};
    Signal<bool> visible{true};
    f.services.mount(Content{[&] {
        QRCode(
            QRCodeProps{}.value(u8"LOGO").icon(logo).iconSize(QRCodeIconSize{16, 16}).visible(visible).onIconLoad([&] {
                ++loaded;
            }));
    }});
    sync(f);
    const auto id = f.services.qrcode().mounted().front();
    expect(pending.size() == 1, "Logo loader not started");
    bool foreign = true;
    std::thread worker([&] { foreign = pending[0]({pixels, {}}); });
    worker.join();
    expect(!foreign, "Foreign logo completion accepted");
    visible.set(false);
    sync(f);
    expect(cancels == 1 && !pending[0]({pixels, {}}), "Hidden logo accepted stale result");
    visible.set(true);
    sync(f);
    expect(pending.size() == 2 && pending[1]({pixels, {}}), "Logo resume failed");
    sync(f);
    expect(loaded == 1 && f.services.images().live_count() == 1 && !pending[1]({pixels, {}}),
           "Logo publish/duplicate completion failed");
    expect(f.services.modal().mounted().empty(), "Nonpreview QR logo mounted unexpected Modal");
    f.services.set_window_active(false);
    sync(f);
    f.services.set_window_active(true);
    sync(f);
    clean(f);
    expect(!pending.back()({pixels, {}}), "Retired completion accepted");
    Fixture g;
    Signal<ImageSource> src{ImageSource{loader}};
    auto replacement = std::make_shared<ImageLoader>();
    replacement->start = [&](ImageCompletion) {
        return [&] {
            src.set(ImageSource{pixels});
        };
    };
    src.set(ImageSource{replacement});
    g.services.mount(Content{[&] { QRCode(QRCodeProps{}.value(u8"CANCEL").icon(src)); }});
    sync(g);
    src.set(ImageSource{});
    sync(g);
    expect(g.services.qrcode().snapshot(g.services.qrcode().mounted().front()).logo_status == ImageStatus::Ready,
           "New source from cancel lost authority");
    clean(g);
}

void errors() {
    Fixture f;
    Signal<QRCodeValue> value{QRCodeValue{}};
    int errors{};
    QRCodeRef ref;
    f.services.mount(
        Content{[&] { QRCode(QRCodeProps{}.value(value).ref(ref).onError([&](const QRCodeError&) { ++errors; })); }});
    sync(f);
    expect(errors == 1 && !ref.data(), "Empty code not represented as error");
    value.set(QRCodeValue{u8"RECOVER"});
    sync(f);
    expect(ref.data().has_value() && !f.services.qrcode().snapshot(f.services.qrcode().mounted().front()).error,
           "Error recovery failed");
    clean(f);
    Fixture g;
    Signal<QRCodeValue> v{QRCodeValue{u8"x"}};
    runtime::ComponentId owner;
    g.services.mount(Content{
        [&] { QRCode(QRCodeProps{}.value(v).onError([&](const QRCodeError&) { g.services.destroy(owner); })); }});
    sync(g);
    owner = g.services.qrcode().mounted().front();
    v.set(QRCodeValue{});
    sync(g);
    expect(g.services.qrcode().mounted().empty(), "Self-destroying error did not retire exact generation");
    clean(g);
    Fixture h;
    auto renderer = std::make_shared<QRCodeStatusRenderer>();
    renderer->render = [](const QRCodeStatusContext&) -> QRCodeStatusContent {
        throw std::runtime_error("status factory");
    };
    bool threw{};
    try {
        h.services.mount(Content{[&] { QRCode(QRCodeProps{}.value(u8"THROW").statusRender(renderer)); }});
    } catch (...) {
        threw = true;
    }
    expect(threw, "Throwing status renderer was swallowed");
    clean(h);
}

void idle() {
    Fixture f;
    f.services.mount(Content{[] { QRCode(QRCodeProps{}.value(u8"IDLE")); }});
    sync(f);
    sync(f);
    const auto creates = f.services.surfaces().diagnostics().creates;
    const auto updates = f.services.surfaces().diagnostics().material_updates;
    const auto revision = f.services.frame_request_revision();
    const auto runs = f.services.components().mount_runs();
    for (int i = 0; i < 10; ++i) {
        sync(f);
    }
    expect(f.services.surfaces().diagnostics().creates == creates &&
               f.services.surfaces().diagnostics().material_updates == updates &&
               f.services.components().mount_runs() == runs,
           "Idle QR rebuilt scene/content");
    expect(f.services.frame_request_revision() == revision && !f.services.next_frame_deadline(),
           "Idle QR scheduled frames");
    clean(f);
}

void rich() {
    Fixture f;
    InputRef editor;
    Signal<QRCodeStatus> status{QRCodeStatus::Expired};
    Signal<std::shared_ptr<const QRCodeStatusRenderer>> renderer{std::shared_ptr<const QRCodeStatusRenderer>{}};
    std::function<bool()> old_action;
    int runs{};
    int refreshes{};
    auto factory = std::make_shared<QRCodeStatusRenderer>();
    factory->render = [&](const QRCodeStatusContext& context) {
        ++runs;
        old_action = context.refresh;
        return QRCodeStatusContent{[&] { Input(InputProps{}.ref(editor).defaultValue(u8"draft")); }};
    };
    renderer.set(std::shared_ptr<const QRCodeStatusRenderer>{factory});
    f.services.mount(Content{[&] {
        QRCode(QRCodeProps{}.value(u8"RICH").status(status).statusRender(renderer).onRefresh([&] { ++refreshes; }));
    }});
    sync(f);
    expect(editor.focus(), "Custom QR status editor did not focus");
    const auto stamp = f.inputs.sessions().active();
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"retained draft"}, stamp})),
           "Custom status editor did not edit");
    expect(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "Custom status preedit failed");
    status.set(QRCodeStatus::Active);
    sync(f);
    expect(!f.inputs.sessions().active().valid() && !editor.focus() && !old_action(),
           "Hidden QR status retained IME/action");
    status.set(QRCodeStatus::Expired);
    sync(f);
    expect(editor.focus() && runs == 1 && old_action() && refreshes == 1, "QR status rebuilt factory or lost action");
    const auto old = old_action;
    auto next = std::make_shared<QRCodeStatusRenderer>();
    next->render = [&](const QRCodeStatusContext& context) {
        ++runs;
        old_action = context.refresh;
        return QRCodeStatusContent{[&] { Input(InputProps{}.ref(editor).defaultValue(u8"replacement")); }};
    };
    renderer.set(std::shared_ptr<const QRCodeStatusRenderer>{next});
    sync(f);
    expect(runs == 2 && !old() && editor.bound() && old_action(),
           "Retired QR status factory action/ref remained authoritative");
    clean(f);
}

void form_scope() {
    Fixture f;
    FormRef form;
    FormField<String> outer{String{u8"outer"}};
    FormField<String> nested{String{u8"nested"}};
    InputRef private_editor;
    InputRef nested_editor;
    InputRef sibling;
    int submitted{};
    auto renderer = std::make_shared<QRCodeStatusRenderer>();
    renderer->render = [&](const QRCodeStatusContext&) {
        return QRCodeStatusContent{[&] {
            Input(InputProps{}.ref(private_editor));
            FormItem(FormItemProps{}.field(u8"nested", nested),
                     FormItemContent{[&] { Input(InputProps{}.ref(nested_editor)); }});
        }};
    };
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](const FormResult&) { ++submitted; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                              QRCode(QRCodeProps{}
                                         .value(u8"FORM")
                                         .status(QRCodeStatus::Expired)
                                         .size(300)
                                         .statusRender(renderer));
                              Input(InputProps{}.ref(sibling));
                          }});
             }});
    }});
    sync(f);
    expect(private_editor.focus(), "QR private status editor did not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"private"}, f.inputs.sessions().active()})),
           "QR private editor failed");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    expect(outer.get() == String{u8"outer"} && submitted == 0, "QR private controls claimed outer Form/Enter");
    expect(nested_editor.focus({InputFocusCursor::All}), "QR nested FormItem did not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"nested changed"}, f.inputs.sessions().active()})),
           "QR nested field failed");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    expect(nested.get() == String{u8"nested changed"} && submitted == 1, "QR rich nested FormItem lost binding/Enter");
    expect(sibling.focus({InputFocusCursor::All}), "QR sibling editor did not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"outer changed"}, f.inputs.sessions().active()})) &&
               outer.get() == String{u8"outer changed"},
           "QR claimed sibling Form binding");
    clean(f);
}

void paint_errors() {
    Fixture f;
    Signal<Color> fg{Color{0, 0, 0, 1}};
    Signal<float> size{160};
    int errors{};
    f.services.mount(Content{[&] {
        QRCode(QRCodeProps{}
                   .value(u8"PAINT")
                   .color(fg)
                   .bgColor(Color{1, 1, 1, 1})
                   .size(size)
                   .onError([&](const QRCodeError& e) {
                       if (e.code == QRCodeErrorCode::Paint) {
                           ++errors;
                       }
                   }));
    }});
    sync(f);
    const auto id = f.services.qrcode().mounted().front();
    fg.set(Color{1, 1, 1, 1});
    sync(f);
    expect(f.services.qrcode().snapshot(id).modules.empty() && errors == 1,
           "Invalid contrast produced valid-looking code");
    sync(f);
    expect(errors == 1, "Invalid colors produced repeated errors");
    fg.set(Color{0, 0, 0, 1});
    sync(f);
    expect(!f.services.qrcode().snapshot(id).error && !f.services.qrcode().snapshot(id).modules.empty(),
           "Paint error recovery failed");
    bool threw{};
    try {
        size.set(std::numeric_limits<float>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "Nonfinite size accepted");
    size.set(32);
    sync(f);
    clean(f);
}

void throwing_callbacks() {
    Fixture f;
    QRCodeRef ref;
    f.services.mount(Content{[&] {
        QRCode(QRCodeProps{}.value(u8"THROW").status(QRCodeStatus::Expired).ref(ref).onRefresh([] {
            throw std::runtime_error("refresh");
        }));
    }});
    sync(f);
    bool threw{};
    try {
        (void)ref.refresh();
    } catch (...) {
        threw = true;
    }
    expect(threw && !ref.bound() && f.services.qrcode().mounted().empty(),
           "Throwing refresh did not retire exact QR owner");
    clean(f);
    Fixture g;
    Signal<QRCodeStatus> status{QRCodeStatus::Expired};
    runtime::ComponentId owner;
    Scope observers;
    std::optional<Effect> effect;
    auto renderer = std::make_shared<QRCodeStatusRenderer>();
    renderer->render = [&](const QRCodeStatusContext& context) {
        effect.emplace(ryn::effect(observers, [&, state = context.disabled] {
            if (state.get()) {
                g.services.destroy(owner);
            }
        }));
        return QRCodeStatusContent{[] { Text(u8"observed status"); }};
    };
    Signal<bool> disabled{false};
    g.services.mount(Content{
        [&] { QRCode(QRCodeProps{}.value(u8"REENTRANT").status(status).disabled(disabled).statusRender(renderer)); }});
    sync(g);
    owner = g.services.qrcode().mounted().front();
    disabled.set(true);
    sync(g);
    expect(g.services.qrcode().mounted().empty(), "Reentrant custom status Signal did not retire owner safely");
    clean(g);
}

void theme() {
    ThemeConfig config;
    config.qrcode.tokens.foreground = Color::rgba8(12, 23, 34);
    config.qrcode.tokens.background = Color::rgba8(240, 241, 242);
    config.qrcode.tokens.padding = dp(2);
    auto t = resolve_theme(config);
    expect(t.qrcode().colors.foreground == *config.qrcode.tokens.foreground && t.qrcode().metrics.padding == 2,
           "QR Theme override ignored");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    expect(resolve_theme(dark).qrcode().colors.foreground != resolve_theme({}).qrcode().colors.foreground,
           "QR Theme dark contrast stale");
    Fixture f;
    Signal<ThemeConfig> cfg{config};
    f.services.mount(Content{
        [&] { Theme(ThemeProps{}.config(cfg), ThemeContent{[] { QRCode(QRCodeProps{}.value(u8"THEME")); }}); }});
    sync(f);
    auto id = f.services.qrcode().mounted().front();
    auto q = f.services.qrcode().snapshot(id).modules.front();
    expect(q.color[0] == 12 / 255.F, "QR scene ignored owned foreground token");
    config.qrcode.tokens.foreground = Color::rgba8(90, 40, 20);
    cfg.set(config);
    sync(f);
    q = f.services.qrcode().snapshot(id).modules.front();
    expect(q.color[0] == 90 / 255.F, "QR Theme subscription stale");
    clean(f);
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "basic";
        if (name == "basic") {
            basic();
        } else if (name == "geometry") {
            geometry();
        } else if (name == "async") {
            async();
        } else if (name == "errors") {
            errors();
        } else if (name == "idle") {
            idle();
        } else if (name == "rich") {
            rich();
        } else if (name == "form_scope") {
            form_scope();
        } else if (name == "paint_errors") {
            paint_errors();
        } else if (name == "throwing_callbacks") {
            throwing_callbacks();
        } else if (name == "theme") {
            theme();
        } else {
            throw std::runtime_error("unknown case");
        }
        std::cout << checks << " QRCode checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
