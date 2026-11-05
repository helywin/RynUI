#include "component/upload_component.hpp"
#include "component/image_component.hpp"
#include "component/form_component.hpp"
#include "support/timeline_fixture.hpp"
#include <ryn/form_upload.hpp>
#include <fstream>
#include <thread>
#include <limits>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool v, const char* message) {
    ++checks;
    check(v, message);
}

std::shared_ptr<const NativeFile> fixture(String name = String{u8"colors.png"}) {
    std::ifstream stream(std::string(RYNUI_IMAGE_FIXTURES) + "/colors.png", std::ios::binary);
    std::vector<char> bytes{std::istreambuf_iterator<char>(stream), {}};
    auto file = std::make_shared<NativeFile>();
    file->name = std::move(name);
    file->mime_type = u8"image/png";
    file->bytes =
        std::make_shared<const std::vector<std::byte>>(reinterpret_cast<const std::byte*>(bytes.data()),
                                                       reinterpret_cast<const std::byte*>(bytes.data() + bytes.size()));
    file->size = file->bytes->size();
    file->locator = u8"workspace-fixture";
    return file;
}

class MockFiles final : public NativeFileServices {
public:
    std::vector<FileSelectionCompletion> requests;
    std::vector<FileDialogOptions> options;
    std::vector<std::vector<String>> paths;
    int cancelled{};
    int pastes{};

    FileServiceCancel choose(const FileDialogOptions& v, FileSelectionCompletion done) override {
        options.push_back(v);
        requests.push_back(std::move(done));
        return [this] {
            ++cancelled;
        };
    }

    FileServiceCancel resolve(const std::vector<String>& v, bool directory, FileSelectionCompletion done) override {
        paths.push_back(v);
        options.push_back({true, directory, {}});
        requests.push_back(std::move(done));
        return [this] {
            ++cancelled;
        };
    }

    FileServiceCancel paste(FileSelectionCompletion done) override {
        ++pastes;
        requests.push_back(std::move(done));
        return [this] {
            ++cancelled;
        };
    }
};

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

void basic() {
    Fixture f;
    MockFiles native;
    f.services.bind_file_services(&native, 7);
    UploadRef ref;
    int downloads{};
    int compositions{};
    f.services.mount(Content{[&] {
        ++compositions;
        Upload(UploadProps{}
                   .ref(ref)
                   .multiple()
                   .accept(u8".png")
                   .listType(UploadListType::Picture)
                   .onDownload([&](const UploadFile&) { ++downloads; })
                   .layout(LayoutStyle{}.width(dp(520))));
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    expect(ref.bound() && ref.focus(), "Upload native ref/focus failed");
    key(f, Key::enter);
    expect(native.requests.size() == 1 && native.options[0].multiple, "Keyboard did not open typed chooser");
    expect(native.requests[0]({{fixture()}, {}, {}}), "Chooser result rejected");
    expect(!native.requests[0]({{fixture()}, {}, {}}), "Duplicate chooser accepted");
    sync(f);
    auto view = f.services.upload().snapshot(id);
    expect(view.files.size() == 1 && view.rows.size() == 1 && view.rows[0].image.valid() &&
               f.services.images().live_count() == 1,
           "Picture list has no actual decoded image");
    expect(f.services.upload().preview(id, view.files[0].uid), "Default preview rejected");
    sync(f);
    expect(f.services.image().snapshot(view.preview_group).open && f.services.images().live_count() == 2,
           "Default preview did not show real image");
    key(f, Key::escape);
    expect(f.services.upload().download(id, view.files[0].uid) && downloads == 1, "Download callback lost");
    expect(f.services.upload().remove(id, view.files[0].uid), "Remove failed");
    sync(f);
    expect(f.services.upload().snapshot(id).files.empty() && f.services.images().live_count() == 0,
           "Remove retained file/image");
    expect(compositions == 1, "Upload reran unrelated component");
    f.services.dispose();
    expect(!ref.bound() && f.services.images().live_count() == 0 && f.services.interactions().size() == 0 &&
               f.services.rounded_effects().live_count() == 0,
           "Upload resources leaked");
}

void transport() {
    Fixture f;
    UploadRef ref;
    std::vector<UploadRequest> requests;
    int cancels{};
    f.services.mount(Content{[&] {
        Upload(UploadProps{}.ref(ref).multiple().customRequest([&](const UploadRequest& r) {
            requests.push_back(r);
            return [&] {
                ++cancels;
            };
        }));
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    expect(ref.select({fixture(), fixture(u8"second.png")}) && requests.size() == 2, "Multiple requests missing");
    sync(f);
    auto view = f.services.upload().snapshot(id);
    const auto first = view.rows[0].component;
    expect(requests[0].progress(31) && requests[1].progress(72), "Independent progress failed");
    sync(f);
    view = f.services.upload().snapshot(id);
    expect(view.files[0].percent == 31 && view.files[1].percent == 72 && view.rows[0].component == first,
           "Progress changed identity or wrong file");
    expect(requests[0].failure(u8"Transport error"), "Failure rejected");
    expect(ref.retry(view.files[0].uid) && requests.size() == 3, "Retry failed");
    expect(requests[2].success({String{u8"Application response"}, {}}), "Success rejected");
    expect(ref.abort(view.files[1].uid) && cancels == 1 && !requests[1].success({}), "Abort accepted stale transport");
    sync(f);
    view = f.services.upload().snapshot(id);
    expect(view.files[0].status == UploadFileStatus::Done && view.files[1].status == UploadFileStatus::Cancelled,
           "Transport status wrong");
    bool foreign = true;
    std::thread thread([&] { foreign = ref.select({fixture()}); });
    thread.join();
    expect(!foreign, "Foreign UploadRef mutated Core");
}

void controlled() {
    Fixture f;
    UploadRef ref;
    Signal<UploadFiles> files{UploadFiles{}};
    bool accept{};
    int sent{};
    f.services.mount(Content{[&] {
        Upload(UploadProps{}
                   .ref(ref)
                   .fileList(files)
                   .multiple()
                   .maxCount(2)
                   .customRequest([&](const UploadRequest&) {
                       ++sent;
                       return UploadCancel{};
                   })
                   .onChange([&](const UploadChange& c) {
                       if (accept) {
                           files.set(c.file_list);
                       }
                   }));
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    ref.select({fixture()});
    expect(files.get().empty() && sent == 0, "Controlled refusal was overwritten");
    accept = true;
    ref.select({fixture(), fixture(u8"second.png"), fixture(u8"third.png")});
    sync(f);
    auto v = f.services.upload().snapshot(id);
    expect(v.files.size() == 2 && sent == 2, "maxCount/controlled selection wrong");
    const auto a = v.rows[0].component;
    const auto b = v.rows[1].component;
    expect(f.services.upload().preview(id, v.files[0].uid), "Controlled Upload preview did not open");
    sync(f);
    const auto group = v.preview_group;
    expect(f.services.image().action(group, ImageAction::ZoomIn), "Upload preview zoom failed");
    sync(f);
    const auto transform = f.services.image().snapshot(group).transform;
    auto reversed = files.get();
    std::reverse(reversed.begin(), reversed.end());
    files.set(reversed);
    sync(f);
    v = f.services.upload().snapshot(id);
    expect(v.rows[0].component == b && v.rows[1].component == a, "UID reorder remounted file rows");
    expect(f.services.image().snapshot(group).current == 1 && f.services.image().snapshot(group).transform == transform,
           "UID reorder changed the open preview or reset its transform");

    files.set({});
    sync(f);
    expect(f.services.upload().snapshot(id).pending == 0, "Controlled removal retained requests");
}

void before() {
    Fixture f;
    UploadRef ref;
    std::vector<UploadBeforeCompletion> done;
    int transformed{};
    int requests{};
    f.services.mount(Content{[&] {
        Upload(UploadProps{}
                   .ref(ref)
                   .multiple()
                   .beforeUpload([&](const UploadFile&, const UploadFiles&, UploadBeforeCompletion c) {
                       done.push_back(c);
                       return UploadCancel{};
                   })
                   .transformFile([&](const UploadFile&, const UploadFiles&, UploadBeforeCompletion c) {
                       ++transformed;
                       c({UploadBeforeDecision::Upload, fixture(u8"transformed.png")});
                       return UploadCancel{};
                   })
                   .customRequest([&](const UploadRequest&) {
                       ++requests;
                       return UploadCancel{};
                   }));
    }});
    sync(f);
    ref.select({fixture(u8"ignored.png"), fixture()});
    expect(done.size() == 2 && requests == 0, "beforeUpload was bypassed");
    expect(done[1]({}), "Async before accepted no result");
    expect(requests == 0, "Out of order before changed list");
    expect(done[0]({UploadBeforeDecision::Ignore, {}}), "Ignore rejected");
    sync(f);
    const auto v = f.services.upload().snapshot(f.services.upload().mounted().front());
    expect(v.files.size() == 1 && v.files[0].name == String{u8"transformed.png"} && transformed == 1 && requests == 1,
           "Ignore/transform did not change actual request");
    Fixture g;
    UploadRef pending;
    UploadBeforeCompletion query;
    g.services.mount(Content{[&] {
        Upload(UploadProps{}
                   .ref(pending)
                   .beforeUpload([&](const UploadFile&, const UploadFiles&, UploadBeforeCompletion done) {
                       query = done;
                       return UploadCancel{};
                   })
                   .transformFile([](const UploadFile&, const UploadFiles&, UploadBeforeCompletion) -> UploadCancel {
                       throw std::runtime_error("async transform");
                   }));
    }});
    sync(g);
    pending.select({fixture()});
    bool threw{};
    try {
        query({});
    } catch (...) {
        threw = true;
    }
    expect(threw && !pending.bound() && g.services.upload().mounted().empty() && g.services.images().live_count() == 0,
           "Asynchronous throwing transform did not retire its owner");
}

void dialog() {
    Fixture f;
    MockFiles native;
    f.services.bind_file_services(&native, 9);
    UploadRef ref;
    Signal<bool> visible{true};
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        auto& b = runtime::require_component_build_context();
        owner = b.mount_component<int>(0);
        const auto node = b.root(owner);
        f.services.layout().set_layout(node, layout::BoxLayout{{}, true});
        b.on_resource_cleanup(owner, [&f, node] { f.services.layout().remove_layout(node); });
        b.mount_slot(owner, Content{[&] { Upload(UploadProps{}.ref(ref).visible(visible).directory()); }});
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    expect(ref.open() && native.options[0].directory, "Directory chooser options missing");
    ref.open();
    expect(native.cancelled == 1 && !native.requests[0]({{fixture()}, {}, {}}), "Later open accepted old chooser");
    bool foreign = true;
    std::thread t([&] { foreign = native.requests[1]({{fixture()}, {}, {}}); });
    t.join();
    expect(!foreign, "Chooser completion accepted foreign thread");
    visible.set(false);
    sync(f);
    expect(native.cancelled == 2 && !native.requests[1]({{fixture()}, {}, {}}), "Hidden Upload accepted chooser");
    visible.set(true);
    sync(f);
    ref.open();
    f.services.destroy(id);
    expect(!native.requests[2]({{fixture()}, {}, {}}), "Retired Upload accepted chooser");
    f.services.append_slot(owner, Content{[&] { Upload(UploadProps{}.ref(ref)); }});
    sync(f);
    expect(!native.requests[2]({{fixture()}, {}, {}}), "Late chooser mutated reused UploadRef binding");
    expect(ref.bound() && f.services.upload().snapshot(f.services.upload().mounted().front()).files.empty(),
           "Old chooser mutated new binding");
}

void drop() {
    Fixture f;
    MockFiles native;
    f.services.bind_file_services(&native, 7);
    int drops{};
    Signal<LogicalLength> width{dp(400)};
    f.services.mount(Content{[&] {
        Upload(UploadProps{}
                   .type(UploadType::Drag)
                   .onDrop([&](const NativeFiles&) { ++drops; })
                   .layout(LayoutStyle{}.width(width)));
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    const auto r = bounds(f, id);
    const float x = r.x + 10;
    const float y = r.y + 10;
    expect(!f.services.dispatch_file_drop({FileDropAction::File, String{u8"fixture"}, x, y, 8}),
           "Foreign window accepted drop");
    expect(!f.services.dispatch_file_drop({FileDropAction::File, String{u8"fixture"}, r.x + r.width + 10, y, 7}),
           "Outside region accepted drop");
    expect(f.services.dispatch_file_drop({FileDropAction::Position, {}, x, y, 7}), "Drop target missing");
    expect(f.services.upload().snapshot(id).dragging, "Drop highlight state missing");
    expect(f.services.dispatch_file_drop({FileDropAction::File, String{u8"workspace-fixture"}, x, y, 7}),
           "Owned path not routed");
    expect(f.services.dispatch_file_drop({FileDropAction::Complete, {}, 0, 0, 7}) && native.paths.size() == 1,
           "Drop did not resolve through host");
    expect(native.requests.back()({{fixture()}, {}, {}}), "Drop completion rejected");
    sync(f);
    expect(drops == 1 && f.services.upload().snapshot(id).files.size() == 1, "Drop callback/selection missing");
    width.set(dp(0));
    expect(!f.services.dispatch_file_drop({FileDropAction::File, String{u8"fixture"}, x, y, 7}),
           "Drop used geometry before pending layout");
    width.set(dp(400));
    sync(f);
    f.services.set_input_blocked(id, true);
    sync(f);
    expect(!f.services.dispatch_file_drop({FileDropAction::File, String{u8"fixture"}, x, y, 7}),
           "Blocked Upload accepted drop");
    f.services.set_input_blocked(id, false);
    f.services.set_window_active(false);
    expect(!f.services.dispatch_file_drop({FileDropAction::File, String{u8"fixture"}, x, y, 7}),
           "Inactive window accepted drop");
}

void paste() {
    Fixture f;
    MockFiles native;
    f.services.bind_file_services(&native, 7);
    UploadRef ref;
    f.services.mount(Content{[&] { Upload(UploadProps{}.ref(ref).pastable()); }});
    sync(f);
    ref.focus();
    key(f, Key::v, KeyModifier::control);
    expect(native.pastes == 1, "Native file paste not routed");
    expect(native.requests[0]({{fixture()}, {}, {}}), "Paste completion rejected");
    sync(f);
    expect(f.services.upload().snapshot(f.services.upload().mounted().front()).files.size() == 1,
           "Paste did not select file");
}

void rich() {
    Fixture f;
    UploadRef ref;
    int mounts{};
    InputRef editor;
    Signal<bool> disabled{false};
    auto render = std::make_shared<UploadItemRenderer>();
    render->render = [&](Signal<UploadFile>, const UploadItemActions&) {
        ++mounts;
        Input(InputProps{}.ref(editor).defaultValue(u8"retained draft"));
    };
    Signal<std::shared_ptr<const UploadItemRenderer>> renderer{render};
    Signal<UploadListType> type{UploadListType::Picture};
    std::vector<UploadRequest> requests;
    f.services.mount(Content{[&] {
        Upload(UploadProps{}.ref(ref).disabled(disabled).itemRender(renderer).listType(type).customRequest(
            [&](const UploadRequest& r) {
                requests.push_back(r);
                return UploadCancel{};
            }));
    }});
    sync(f);
    ref.select({fixture()});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    const auto content = f.services.upload().snapshot(id).rows.front().content;
    expect(editor.focus({InputFocusCursor::End}), "Rich file draft could not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8" edited"}, f.inputs.sessions().active()})),
           "Rich file draft edit failed");
    const auto input = f.inputs.mounted_inputs().front();
    const auto draft = std::string(f.inputs.display_snapshot(input.component).text);
    requests[0].progress(20);
    type.set(UploadListType::PictureCard);
    sync(f);
    expect(mounts == 1 && f.services.upload().snapshot(id).rows.front().content == content,
           "Progress/list type remounted item draft");
    expect(std::string(f.inputs.display_snapshot(input.component).text) == draft,
           "Progress replaced actual file draft");
    disabled.set(true);
    sync(f);
    expect(!editor.focus() && !f.inputs.sessions().active().valid(), "Disabled Upload retained rich editor input/IME");
    disabled.set(false);
    sync(f);
    expect(editor.focus() && std::string(f.inputs.display_snapshot(input.component).text) == draft,
           "Resumed Upload lost retained draft");
    auto bad = std::make_shared<UploadItemRenderer>();
    bad->render = [](Signal<UploadFile>, const UploadItemActions&) {
        throw std::runtime_error("render");
    };
    bool threw{};
    try {
        renderer.set(bad);
    } catch (...) {
        threw = true;
    }
    expect(threw && !ref.bound() && f.services.images().live_count() == 0 &&
               blocked_roots(f.services.interactions()).empty(),
           "Throwing renderer did not retire failed owner");
    Fixture g;
    UploadRef replacement;
    Signal<std::shared_ptr<const UploadItemRenderer>> mode{std::shared_ptr<const UploadItemRenderer>{}};
    g.services.mount(
        Content{[&] { Upload(UploadProps{}.ref(replacement).listType(UploadListType::Picture).itemRender(mode)); }});
    sync(g);
    replacement.select({fixture()});
    sync(g);
    const auto owner = g.services.upload().mounted().front();
    const auto old = g.services.upload().snapshot(owner).rows.front();
    auto custom = std::make_shared<UploadItemRenderer>();
    custom->render = [](Signal<UploadFile>, const UploadItemActions&) {
        Input(InputProps{}.defaultValue(u8"new local renderer"));
    };
    mode.set(custom);
    sync(g);
    expect(g.services.upload().snapshot(owner).rows.front().component == old.component &&
               !g.services.upload().snapshot(owner).rows.front().image.valid() &&
               !g.services.components().contains(old.image),
           "Renderer switch retained stale Image/action IDs");
    mode.set({});
    sync(g);
    expect(g.services.upload().snapshot(owner).rows.front().image.valid() && g.inputs.mounted_inputs().empty(),
           "Default renderer did not restore locally");
    std::function<void()> late;
    {
        auto h = std::make_unique<Fixture>();
        UploadRef handle;
        auto render = std::make_shared<UploadItemRenderer>();
        render->render = [&](Signal<UploadFile>, const UploadItemActions& actions) {
            late = actions.remove;
            Text(u8"Retained external action");
        };
        h->services.mount(Content{
            [&] { Upload(UploadProps{}.ref(handle).itemRender(std::shared_ptr<const UploadItemRenderer>{render})); }});
        sync(*h);
        handle.select({fixture()});
        sync(*h);
        expect(bool(late), "Public item action was not supplied");
    }
    late();
    expect(true, "Retired item action threw");
}

void lifecycle() {
    Fixture f;
    UploadRef ref;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<bool> visible{true};
    std::vector<UploadRequest> requests;
    int cancels{};
    f.services.mount(Content{[&] {
        Upload(UploadProps{}.ref(ref).disabled(disabled).readOnly(read_only).visible(visible).customRequest(
            [&](const UploadRequest& r) {
                requests.push_back(r);
                return [&] {
                    ++cancels;
                };
            }));
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    ref.select({fixture()});
    disabled.set(true);
    sync(f);
    expect(cancels == 1 && !requests[0].progress(20) && !ref.select({fixture()}), "Disabled work not cancelled");
    disabled.set(false);
    read_only.set(true);
    sync(f);
    expect(!ref.open() && !ref.select({fixture()}), "Read only allowed mutation");
    read_only.set(false);
    visible.set(false);
    sync(f);
    expect(!ref.focus() && !ref.open(), "Hidden Upload accepted focus/dialog");
    visible.set(true);
    sync(f);
    ref.retry(f.services.upload().snapshot(id).files.front().uid);
    f.services.set_window_active(false);
    sync(f);
    expect(cancels == 2 && !requests.back().success({}), "Window inactive retained transport");
}

void failures() {
    Fixture f;
    UploadRef ref;
    f.services.mount(Content{[&] {
        Upload(UploadProps{}.ref(ref).onChange(
            [&](const UploadChange&) { f.services.destroy(f.services.upload().mounted().front()); }));
    }});
    sync(f);
    expect(ref.select({fixture()}) && !ref.bound() && f.services.images().live_count() == 0,
           "Self-destroying Upload callback leaked");
    Fixture g;
    UploadRef throwing;
    UploadRequest pending;
    g.services.mount(Content{[&] {
        Upload(UploadProps{}
                   .ref(throwing)
                   .customRequest([&](const UploadRequest& r) {
                       pending = r;
                       return UploadCancel{};
                   })
                   .onChange([](const UploadChange& c) {
                       if (c.reason == UploadChangeReason::Progress) {
                           throw std::runtime_error("progress");
                       }
                   }));
    }});
    sync(g);
    throwing.select({fixture()});
    bool threw{};
    try {
        pending.progress(50);
    } catch (...) {
        threw = true;
    }
    expect(threw && !throwing.bound() && g.services.images().live_count() == 0,
           "Throwing async callback did not retire Upload");
}

void theme() {
    Fixture f;
    UploadRef ref;
    ThemeConfig config;
    config.upload.tokens.thumbnail_size = dp(72);
    config.upload.tokens.radius = dp(12);
    Signal<ThemeConfig> tokens{config};
    Signal<UploadListType> type{UploadListType::PictureCircle};
    Signal<bool> rtl{false};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(tokens), ThemeContent{[&] {
                  Upload(
                      UploadProps{}.ref(ref).listType(type).rtl(rtl).multiple().layout(LayoutStyle{}.width(dp(400))));
              }});
    }});
    sync(f);
    ref.select({fixture(), fixture(u8"second.png")});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    auto v = f.services.upload().snapshot(id);
    const auto image = v.rows[0].image;
    expect(bounds(f, image).width == 72 && f.services.image().snapshot(image).image.has_value(),
           "Upload thumbnail token not applied");
    expect(f.services.images().find(*f.services.image().snapshot(image).image)->instance.circle_clip.has_value(),
           "Picture circle did not use shared image clipping");
    rtl.set(true);
    sync(f);
    v = f.services.upload().snapshot(id);
    expect(v.rows[0].bounds.x > v.rows[1].bounds.x, "Picture card RTL order wrong");
    config.upload.tokens.thumbnail_size = dp(80);
    tokens.set(config);
    sync(f);
    expect(f.services.upload().snapshot(id).rows[0].image == image && bounds(f, image).width == 80,
           "Upload theme remounted thumbnail");
}

void idle() {
    Fixture f;
    f.services.mount(Content{[] { Upload(); }});
    sync(f);
    f.frames.consume_request();
    sync(f);
    f.frames.consume_request();
    sync(f);
    expect(!f.frames.pending() && !f.services.next_frame_deadline(), "Idle Upload requested a frame");
    f.services.dispose();
    expect(f.services.images().live_count() == 0 && f.services.interactions().size() == 0 &&
               f.services.rounded_effects().live_count() == 0,
           "Idle Upload leaked");
}

void form() {
    Fixture f;
    UploadRef upload;
    FormRef form;
    FormField<String> field{String{}};
    field.required();
    UploadFiles domain;
    UploadRequest request;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).disabled(disabled).readOnly(read_only), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"attachment", field).control(upload), FormItemContent{[&] {
                              UploadFormMapping<String> map;
                              map.decode = [&](const String& value) {
                                  return value.empty() ? UploadFiles{} : domain;
                              };
                              map.encode = [&](const UploadFiles& value) {
                                  domain = value;
                                  return value.empty() ? String{} : value[0].uid;
                              };
                              Upload(upload_form_props(field, std::move(map))
                                         .ref(upload)
                                         .customRequest([&](const UploadRequest& value) {
                                             request = value;
                                             return UploadCancel{};
                                         }));
                          }});
             }});
    }});
    sync(f);
    expect(form.validate().has_value() && !form.validate()->valid() && form.submit(),
           "Form did not validate/focus Upload");
    expect(upload.select({fixture()}), "Form Upload select failed");
    sync(f);
    expect(!field.get().empty() && form.validate()->valid(), "Typed Form mapping lost file value");
    const auto uid = field.get();
    expect(request.progress(63), "Form transport progress rejected");
    sync(f);
    expect(field.get() == uid &&
               f.services.upload().snapshot(f.services.upload().mounted().front()).files.front().percent == 63,
           "Unchanged Form UID suppressed actual file progress");
    expect(request.failure(String{u8"failed"}), "Form transport failure rejected");
    sync(f);
    expect(field.get() == uid && domain.front().status == UploadFileStatus::Error &&
               f.services.upload().snapshot(f.services.upload().mounted().front()).files.front().error ==
                   String{u8"failed"},
           "Unchanged Form UID suppressed status/error metadata");
    const auto target = form.getFieldInstance(FormNamePath{u8"attachment"});
    expect(target && target->geometry && target->geometry() && target->geometry()->bounds.width > 0,
           "Upload Form target did not publish actual trigger geometry");
    disabled.set(true);
    sync(f);
    expect(!upload.select({fixture()}) && !target->focus(), "Form disabled did not gate Upload");
    disabled.set(false);
    read_only.set(true);
    sync(f);
    expect(!upload.select({fixture()}), "Form readOnly did not gate Upload");
    read_only.set(false);
    expect(upload.select({fixture()}), "Form reset fixture did not start a live request");
    expect(form.reset(), "Form Upload reset rejected");
    sync(f);
    expect(field.get().empty() && f.services.upload().snapshot(f.services.upload().mounted().front()).files.empty() &&
               !request.success({}),
           "Form reset retained files or accepted stale completion");
    f.services.dispose();
    expect(!upload.bound() && !target->focus() && !target->geometry(), "Retired Form Upload target stayed valid");
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
    auto render = std::make_shared<UploadItemRenderer>();
    render->render = [&](Signal<UploadFile>, const UploadItemActions&) {
        Input(InputProps{}.ref(private_editor));
        FormItem(FormItemProps{}.field(u8"nested", nested),
                 FormItemContent{[&] { Input(InputProps{}.ref(nested_editor)); }});
    };
    UploadFile file;
    file.uid = u8"retained";
    file.name = u8"fixture.png";
    file.origin = fixture();
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](const FormResult&) { ++submitted; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                              Upload(UploadProps{}.defaultFileList({file}).itemRender(
                                  std::shared_ptr<const UploadItemRenderer>{render}));
                              Input(InputProps{}.ref(sibling));
                          }});
             }});
    }});
    sync(f);
    expect(private_editor.focus(), "Upload private editor could not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"private draft"}, f.inputs.sessions().active()})),
           "Upload private draft edit failed");
    key(f, Key::enter);
    expect(outer.get() == String{u8"outer"} && submitted == 0,
           "Upload private editor claimed outer value or implicit Enter");
    expect(nested_editor.focus({InputFocusCursor::All}), "Upload nested FormItem did not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"changed nested"}, f.inputs.sessions().active()})),
           "Upload nested field edit failed");
    key(f, Key::enter);
    expect(nested.get() == String{u8"changed nested"} && submitted == 1,
           "Upload rich nested FormItem lost its binding/Enter scope");
    expect(sibling.focus({InputFocusCursor::All}), "Outer sibling did not focus");
    expect(bool(f.inputs.dispatch(TextCommitted{String{u8"changed outer"}, f.inputs.sessions().active()})),
           "Outer sibling edit failed");
    expect(outer.get() == String{u8"changed outer"}, "Upload private controls stole outer automatic binding");
    f.services.dispose();
    expect(!f.inputs.sessions().active().valid() && f.services.interactions().size() == 0,
           "Upload Form rich controls leaked after retirement");
}

void validation() {
    Fixture f;
    bool threw{};
    try {
        f.services.mount(Content{[] { Upload(UploadProps{}.maxCount(0)); }});
    } catch (...) {
        threw = true;
    }
    expect(threw && f.services.interactions().size() == 0, "Invalid Upload mount leaked resources");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "basic";
        if (mode == "basic") {
            basic();
        } else if (mode == "transport") {
            transport();
        } else if (mode == "controlled") {
            controlled();
        } else if (mode == "before") {
            before();
        } else if (mode == "dialog") {
            dialog();
        } else if (mode == "drop") {
            drop();
        } else if (mode == "paste") {
            paste();
        } else if (mode == "rich") {
            rich();
        } else if (mode == "lifecycle") {
            lifecycle();
        } else if (mode == "failures") {
            failures();
        } else if (mode == "theme") {
            theme();
        } else if (mode == "idle") {
            idle();
        } else if (mode == "form") {
            form();
        } else if (mode == "form_scope") {
            form_scope();
        } else if (mode == "validation") {
            validation();
        } else {
            throw std::runtime_error("Unknown case");
        }
        std::cout << checks << " Upload component checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
