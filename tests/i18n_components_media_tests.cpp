#include "component/image_component.hpp"
#include "component/qrcode_component.hpp"
#include "component/upload_component.hpp"
#include "support/i18n_test.hpp"
#include "support/timeline_fixture.hpp"

#include <ryn/config_provider.hpp>

#include <array>
#include <source_location>
#include <type_traits>

namespace {
using namespace timeline_test;
using ryn_test::i18n::require;

static_assert(std::is_aggregate_v<ImageLocale> && std::is_aggregate_v<UploadLocale> &&
              std::is_aggregate_v<QRCodeLocale> && std::is_aggregate_v<UploadFile> &&
              std::is_aggregate_v<ImagePreviewItem>);

const MessageKey callsite{u8"media.callsite", String{u8"English callsite"}};

MessageCatalog messages() {
    return MessageCatalog({{locales::zh_CN, callsite, String{u8"中文调用处"}}});
}

std::shared_ptr<const ImagePixels> pixels() {
    const std::array<std::byte, 4> rgba{std::byte{200}, std::byte{40}, std::byte{20}, std::byte{255}};
    return ImagePixels::copy(1, 1, 4, rgba);
}

std::shared_ptr<const NativeFile> file(String name = String{u8"业务文件.txt"}) {
    auto result = std::make_shared<NativeFile>();
    result->name = std::move(name);
    result->mime_type = u8"text/plain";
    result->size = 4;
    return result;
}

bool has_text(Fixture& f, runtime::ComponentId owner, StringView expected) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, owner) && f.scene.text_state(text.scene).content() == expected) {
            return true;
        }
    }
    return false;
}

bool has_text(Fixture& f, runtime::ComponentId owner, const String& expected) {
    return has_text(f, owner, expected.view());
}

template <std::size_t N> bool has_text(Fixture& f, runtime::ComponentId owner, const char8_t (&expected)[N]) {
    return has_text(f, owner, String{expected});
}

std::vector<runtime::ComponentId> text_ids(Fixture& f, runtime::ComponentId owner) {
    std::vector<runtime::ComponentId> result;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (descendant(f, text.component, owner)) {
            result.push_back(text.component);
        }
    }
    return result;
}

void clean(Fixture& f, const std::source_location location = std::source_location::current()) {
    f.services.dispose();
    if (f.nodes.size() != 0 || f.services.images().live_count() != 0 || f.services.interactions().size() != 0 ||
        !blocked_roots(f.services.interactions()).empty()) {
        throw std::runtime_error(std::string(location.function_name()) +
                                 ": media retirement leaked resources: nodes=" + std::to_string(f.nodes.size()) +
                                 ", images=" + std::to_string(f.services.images().live_count()) +
                                 ", interactions=" + std::to_string(f.services.interactions().size()) +
                                 ", blocks=" + std::to_string(blocked_roots(f.services.interactions()).size()));
    }
}

void scope_nesting_and_windows() {
    auto first = std::make_unique<Fixture>();
    auto second = std::make_unique<Fixture>();
    Signal<LocaleId> language{locales::zh_CN};
    std::array<runtime::ComponentId, 3> inherited;
    std::array<runtime::ComponentId, 3> nested;
    std::array<runtime::ComponentId, 3> isolated;
    int mounts = 0;
    const auto media = [](Fixture& f, auto& ids) {
        Image(ImageProps{}.preview(false));
        ids[0] = f.services.image().mounted().back();
        Upload();
        ids[1] = f.services.upload().mounted().back();
        QRCode(QRCodeProps{}.value(u8"literal QR payload").status(QRCodeStatus::Expired));
        ids[2] = f.services.qrcode().mounted().back();
    };
    first->services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           ++mounts;
                           media(*first, inherited);
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[&] { media(*first, nested); }});
                       }});
    }});
    second->services.mount(Content{[&] { media(*second, isolated); }});
    sync(*first);
    sync(*second);
    require(has_text(*first, inherited[0], u8"图片不可用") && has_text(*first, inherited[1], u8"选择文件") &&
                has_text(*first, inherited[2], u8"二维码已过期"),
            "real media components did not inherit Chinese defaults");
    require(has_text(*first, nested[0], u8"Image unavailable") && has_text(*first, nested[1], u8"Select files") &&
                has_text(*first, nested[2], u8"QR code expired"),
            "nested English media components used their parent's language");
    const auto identities = text_ids(*first, inherited[2]);
    const auto encoding = first->services.qrcode().snapshot(inherited[2]).encoding_revision;
    language.set(locales::en_US);
    sync(*first);
    require(has_text(*first, inherited[0], u8"Image unavailable") && has_text(*first, inherited[1], u8"Select files") &&
                has_text(*first, inherited[2], u8"QR code expired") && text_ids(*first, inherited[2]) == identities &&
                mounts == 1 && first->services.qrcode().snapshot(inherited[2]).encoding_revision == encoding,
            "language switch missed media defaults, remounted text, or reencoded QR");
    static_cast<void>(second->frames.consume_request());
    const auto requests = second->frames.counters().requests;
    language.set(locales::zh_CN);
    sync(*first);
    require(has_text(*second, isolated[0], u8"Image unavailable") && has_text(*second, isolated[1], u8"Select files") &&
                has_text(*second, isolated[2], u8"QR code expired") && second->frames.counters().requests == requests,
            "media locale crossed window ownership");
    clean(*first);
    clean(*second);
}

void explicit_english_empty_and_slots() {
    Fixture f;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<QRCodeLocale> qr_locale{QRCodeLocale{}};
    ImageLocale image_locale;
    image_locale.error = {};
    UploadLocale upload_locale;
    upload_locale.select = {};
    std::array<runtime::ComponentId, 6> ids;
    runtime::ComponentId default_group;
    int slot_mounts = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                Image(ImageProps{}.preview(false).alt(u8"Image unavailable"));
                ids[0] = f.services.image().mounted().back();
                Image(ImageProps{}.preview(false).alt(u8""));
                ids[1] = f.services.image().mounted().back();
                Image(ImageProps{}.preview(false).locale(image_locale));
                ids[2] = f.services.image().mounted().back();
                Upload(UploadProps{}.locale(upload_locale));
                ids[3] = f.services.upload().mounted().back();
                QRCode(QRCodeProps{}.value(u8"EXPLICIT").status(QRCodeStatus::Expired).locale(qr_locale));
                ids[4] = f.services.qrcode().mounted().back();
                Image(ImageProps{}.preview(false), ImageSlots{.placeholder = ImagePlaceholder{[&] {
                                                                  ++slot_mounts;
                                                                  Text(TextProps{}.content(u8"literal placeholder"));
                                                              }}});
                ids[5] = f.services.image().mounted().back();
                const auto before_group = f.services.image().mounted().size();
                PreviewGroup(
                    PreviewGroupProps{}.items(std::vector<ImagePreviewItem>{{u8"empty-alt", ImageSource{}, String{}}}));
                default_group = f.services.image().mounted()[before_group];
            }});
    }});
    sync(f);
    require(has_text(f, ids[0], u8"Image unavailable") && has_text(f, ids[1], u8"") &&
                !has_text(f, ids[1], u8"图片不可用") && !has_text(f, ids[2], u8"图片不可用") &&
                !has_text(f, ids[3], u8"选择文件") && has_text(f, ids[4], u8"QR code expired") &&
                has_text(f, ids[5], u8"literal placeholder"),
            "explicit English, empty String/Locale or typed slot lost precedence");
    const auto thumbnail = f.services.image().snapshot(default_group).thumbnails.front();
    require(has_text(f, thumbnail, u8"图片不可用"),
            "PreviewGroup forwarded its omitted aggregate alt as an explicit empty Image label");
    QRCodeLocale empty;
    empty.expired = {};
    empty.refresh = {};
    qr_locale.set(empty);
    language.set(locales::en_US);
    sync(f);
    require(!has_text(f, ids[4], u8"QR code expired") && !has_text(f, ids[4], u8"Refresh") && slot_mounts == 1,
            "reactive explicit empty Locale fell back or remounted a slot");
    language.set(locales::zh_CN);
    sync(f);
    require(!has_text(f, ids[4], u8"二维码已过期") && !has_text(f, ids[3], u8"选择文件") &&
                has_text(f, ids[0], u8"Image unavailable") && !has_text(f, ids[1], u8"图片不可用") && slot_mounts == 1,
            "repeated locale switch replaced explicit media overrides");
    clean(f);
}

void image_loading_preview_and_identity() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<ImageSource> source{ImageSource{}};
    ImageRef ref;
    std::vector<ImageCompletion> completions;
    int starts = 0;
    int cancellations = 0;
    int loaded = 0;
    int opens = 0;
    int transforms = 0;
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion done) {
        ++starts;
        completions.push_back(std::move(done));
        return [&] {
            ++cancellations;
        };
    };
    source.set(ImageSource{loader});
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Image(ImageProps{}
                                     .source(source)
                                     .ref(ref)
                                     .onLoad([&] { ++loaded; })
                                     .onOpenChange([&](bool) { ++opens; })
                                     .onTransform([&](ImageTransform, ImageAction) { ++transforms; })
                                     .layout(LayoutStyle{}.width(dp(80)).height(dp(80))));
                       }});
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    const auto texts = text_ids(f, id);
    language.set(locales::zh_CN);
    sync(f);
    require(starts == 1 && cancellations == 0 && has_text(f, id, u8"图片加载中"),
            "locale switch restarted/cancelled the pending Image load");
    require(completions[0]({pixels(), {}}), "valid Image completion after locale change was rejected");
    sync(f);
    require(ref.preview() && loaded == 1, "loaded Image could not open preview");
    sync(f);
    require(ref.action(ImageAction::ZoomIn) && ref.action(ImageAction::RotateRight), "preview transform failed");
    sync(f);
    const auto before = f.services.image().snapshot(id);
    const auto action_texts = text_ids(f, before.actions);
    const auto events = std::array{loaded, opens, transforms};
    language.set(locales::en_US);
    sync(f);
    const auto after = f.services.image().snapshot(id);
    require(after.open && after.transform == before.transform && after.modal == before.modal &&
                after.canvas == before.canvas && after.actions == before.actions && after.image == before.image &&
                text_ids(f, id) == texts && text_ids(f, after.actions) == action_texts &&
                events == std::array{loaded, opens, transforms} && starts == 1 && cancellations == 0 &&
                has_text(f, after.actions, u8"Zoom in") && has_text(f, after.actions, u8"1 / 1"),
            "locale change reset preview state, identity, loader or business callbacks");
    require(ref.preview(false), "preview close failed");
    source.set(ImageSource{});
    source.set(ImageSource{loader});
    sync(f);
    require(starts == 2 && !completions[0]({pixels(), {}}), "old Image completion retained authority");
    clean(f);
    require(cancellations == 1 && !completions[1]({pixels(), {}}) && !ref.bound(),
            "retired Image accepted a late completion");
    language.set(locales::zh_CN);
}

void preview_group_count_and_controlled_state() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    Signal<std::size_t> current{1};
    Signal<bool> open{true};
    int changes = 0;
    int opens = 0;
    const std::vector<ImagePreviewItem> items{{u8"a", ImageSource{pixels()}, u8"业务 A"},
                                              {u8"b", ImageSource{pixels()}, u8"业务 B"}};
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           PreviewGroup(PreviewGroupProps{}
                                            .items(items)
                                            .open(open)
                                            .current(current)
                                            .onChange([&](std::size_t, std::size_t) { ++changes; })
                                            .onOpenChange([&](bool, std::size_t) { ++opens; }));
                       }});
    }});
    sync(f);
    const auto id = f.services.image().mounted().front();
    require(f.services.image().action(id, ImageAction::ZoomIn), "group preview did not open");
    sync(f);
    const auto before = f.services.image().snapshot(id);
    language.set(locales::zh_CN);
    sync(f);
    const auto after = f.services.image().snapshot(id);
    require(after.open && after.current == 1 && after.transform == before.transform &&
                after.thumbnails == before.thumbnails && after.actions == before.actions &&
                has_text(f, after.actions, u8"第 2 张，共 2 张") && after.alt == String{u8"业务 B"} &&
                current.get() == 1 && open.get() && changes == 0 && opens == 0,
            "localized count changed controlled group data, transforms, thumbnails or callbacks");
    clean(f);
}

void compound_preview_locale_precedence() {
    Fixture f;
    auto data = std::make_unique<LocaleData>(LocaleCatalog::builtin().pack(locales::zh_CN).data());
    data->image.close = u8"scope image close";
    data->upload.image_preview.close = u8"unused nested default";
    const LocaleId custom("zh-XX");
    const LocaleCatalog catalog({{custom, LocalePack(std::move(*data)), locales::zh_CN}});
    Signal<LocaleId> language{custom};
    Signal<UploadLocale> explicit_locale{UploadLocale{}};
    auto explicit_value = explicit_locale.get();
    explicit_value.image_preview.close = u8"explicit upload close";
    explicit_locale.set(explicit_value);
    const UploadFile record{.uid = u8"picture", .name = u8"业务图片", .thumbnail = ImageSource{pixels()}};
    std::array<runtime::ComponentId, 2> ids;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog).locale(language), ConfigProviderContent{[&] {
                           Upload(UploadProps{}.defaultFileList({record}));
                           ids[0] = f.services.upload().mounted().back();
                           Upload(UploadProps{}.defaultFileList({record}).locale(explicit_locale));
                           ids[1] = f.services.upload().mounted().back();
                       }});
    }});
    sync(f);
    const auto first = f.services.upload().snapshot(ids[0]);
    const auto second = f.services.upload().snapshot(ids[1]);
    require(has_text(f, first.preview_group, u8"scope image close") &&
                !has_text(f, first.preview_group, u8"unused nested default") &&
                has_text(f, second.preview_group, u8"explicit upload close"),
            "Upload's default Image Locale sealed inheritance or lost explicit nested override");
    language.set(locales::en_US);
    sync(f);
    require(has_text(f, first.preview_group, u8"Close preview") &&
                has_text(f, second.preview_group, u8"explicit upload close") &&
                f.services.upload().snapshot(ids[0]).rows[0].component == first.rows[0].component,
            "compound preview locale switch rebuilt Upload rows or replaced explicit nested Locale");
    clean(f);
}

void upload_async_rows_and_literal_errors() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    UploadRef ref;
    InputRef row_editor;
    UploadBeforeCompletion before;
    std::vector<UploadRequest> requests;
    int cancels = 0;
    int changes = 0;
    int renders = 0;
    auto renderer = std::make_shared<UploadItemRenderer>();
    renderer->render = [&](Signal<UploadFile> record, const UploadItemActions&) {
        ++renders;
        const auto tr = useI18n();
        Text(TextProps{}.content(tr.text(callsite)));
        Text(TextProps{}.content(bind([record] { return record.get().name; })));
        Input(InputProps{}.defaultValue(u8"保留行草稿").ref(row_editor));
    };
    f.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language).messages(messages()), ConfigProviderContent{[&] {
                Upload(UploadProps{}
                           .ref(ref)
                           .beforeUpload([&](const UploadFile&, const UploadFiles&, UploadBeforeCompletion done) {
                               before = std::move(done);
                               return UploadCancel{[&] { ++cancels; }};
                           })
                           .customRequest([&](const UploadRequest& request) {
                               requests.push_back(request);
                               return UploadCancel{[&] { ++cancels; }};
                           })
                           .itemRender(std::shared_ptr<const UploadItemRenderer>{renderer})
                           .onChange([&](const UploadChange&) { ++changes; }));
            }});
    }});
    sync(f);
    const auto id = f.services.upload().mounted().front();
    require(ref.select({file()}), "Upload selection failed");
    language.set(locales::zh_CN);
    require(cancels == 0 && requests.empty() && f.services.upload().snapshot(id).pending == 1,
            "locale switch cancelled the pending beforeUpload job");
    require(before({}), "beforeUpload late completion lost its live request");
    sync(f);
    require(requests.size() == 1 && renders == 1, "Upload row/transport did not mount once");
    const auto row = f.services.upload().snapshot(id).rows.front();
    const auto input = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(input.editor);
    require(row_editor.focus(), "rich Upload row could not focus its retained Input");
    require(static_cast<bool>(editor.replace_range({0, editor.value().size()}, "a中文z")), "row draft setup failed");
    require(row_editor.select(7, 1), "row reverse selection setup failed");
    require(static_cast<bool>(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "row composition setup failed");
    const auto draft = std::string(editor.value());
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto events = changes;
    language.set(locales::en_US);
    sync(f);
    require(f.services.upload().snapshot(id).rows.front().component == row.component &&
                f.services.upload().snapshot(id).rows.front().content == row.content && renders == 1 &&
                f.inputs.mounted_inputs().front().editor == input.editor && editor.value() == draft &&
                editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() && f.inputs.focused(input.component) &&
                editor.history().undo_count == history.undo_count && changes == events && cancels == 0 &&
                has_text(f, row.component, u8"English callsite") && has_text(f, row.component, u8"业务文件.txt"),
            "locale change reconciled Upload rows, reset rich content, cancelled work or emitted onChange");
    require(requests[0].progress(37), "pending transport progress was invalidated by language");
    clean(f);
    require(cancels == 1 && !requests[0].success({}) && !before({}) && !ref.bound(),
            "retired Upload accepted a late gate/transport callback");
    language.set(locales::zh_CN);
}

void upload_errors_controlled_and_open_preview() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    const UploadFile initial{.uid = u8"visible", .name = u8"业务图片", .thumbnail = ImageSource{pixels()}};
    Signal<UploadFiles> files{UploadFiles{initial}};
    UploadRef controlled;
    UploadRef errors;
    int changes = 0;
    std::vector<UploadRequest> requests;
    f.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                Upload(UploadProps{}.fileList(files).ref(controlled).onChange([&](const UploadChange&) { ++changes; }));
                Upload(UploadProps{}.ref(errors).multiple().customRequest([&](const UploadRequest& request) {
                    if (request.file.name == String{u8"builtin"}) {
                        throw 7;
                    }
                    requests.push_back(request);
                    return UploadCancel{};
                }));
            }});
    }});
    sync(f);
    const auto controlled_id = f.services.upload().mounted()[0];
    const auto errors_id = f.services.upload().mounted()[1];
    const auto before = f.services.upload().snapshot(controlled_id);
    require(errors.select({file(u8"builtin"), file(u8"english"), file(u8"empty")}) && requests.size() == 2,
            "error source fixture did not start transports");
    require(requests[0].failure(u8"Upload failed") && requests[1].failure(u8""), "literal failure fixture failed");
    sync(f);
    require(f.services.upload().preview(controlled_id, initial.uid), "Upload preview did not open");
    sync(f);
    require(f.services.image().action(before.preview_group, ImageAction::ZoomIn), "Upload preview transform failed");
    sync(f);
    const auto preview = f.services.image().snapshot(before.preview_group);
    const auto events = changes;
    language.set(locales::zh_CN);
    sync(f);
    const auto after = f.services.upload().snapshot(controlled_id);
    const auto after_preview = f.services.image().snapshot(before.preview_group);
    require(after.files == before.files && files.get() == UploadFiles{initial} &&
                after.rows.front().component == before.rows.front().component && after_preview.open &&
                after_preview.current == preview.current && after_preview.transform == preview.transform &&
                after_preview.actions == preview.actions && changes == events &&
                has_text(f, before.preview_group, u8"关闭预览"),
            "language switch changed Upload controlled data, rows, preview state or callbacks");
    require(has_text(f, errors_id, u8"builtin — 上传失败") && has_text(f, errors_id, u8"english — Upload failed") &&
                has_text(f, errors_id, u8"empty — "),
            "builtin Upload error was not localized or explicit English/empty error was translated");
    const auto records = f.services.upload().snapshot(errors_id).files;
    require(records[0].error_source == UploadErrorSource::Builtin &&
                records[1].error_source == UploadErrorSource::Literal &&
                records[2].error_source == UploadErrorSource::Literal,
            "Upload model lost builtin/literal error provenance");
    clean(f);
}

void qrcode_logo_renderer_reentry_and_retirement() {
    Fixture f;
    Signal<LocaleId> language{locales::en_US};
    std::vector<ImageCompletion> completions;
    int starts = 0;
    int cancels = 0;
    int loads = 0;
    int renders = 0;
    int refreshes = 0;
    int mode = 0;
    runtime::ComponentId id;
    QRCodeRef ref;
    InputRef rich_editor;
    auto loader = std::make_shared<ImageLoader>();
    loader->start = [&](ImageCompletion done) {
        ++starts;
        completions.push_back(std::move(done));
        return [&] {
            ++cancels;
        };
    };
    Signal<ImageSource> icon{ImageSource{loader}};
    auto renderer = std::make_shared<QRCodeStatusRenderer>();
    renderer->render = [&](const QRCodeStatusContext& context) {
        ++renders;
        return QRCodeStatusContent{[&, context] {
            auto& scope = runtime::require_component_build_context().lifetime_scope();
            static_cast<void>(connect_binding(scope, bind([locale = context.locale] { return locale.get(); }),
                                              [&](const QRCodeLocale& value) {
                                                  if (value.expired == String{u8"二维码已过期"} && mode) {
                                                      const auto action = std::exchange(mode, 0);
                                                      if (action == 1) {
                                                          language.set(locales::en_US);
                                                      } else {
                                                          f.services.destroy(id);
                                                      }
                                                  }
                                              }));
            Text(TextProps{}.content(bind([locale = context.locale] { return locale.get().expired; })));
            Input(InputProps{}.defaultValue(u8"QR rich draft").ref(rich_editor));
        }};
    };
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           QRCode(QRCodeProps{}
                                      .value(u8"二维码业务数据 stays literal")
                                      .status(QRCodeStatus::Expired)
                                      .icon(icon)
                                      .statusRender(renderer)
                                      .onRefresh([&] { ++refreshes; })
                                      .onIconLoad([&] { ++loads; })
                                      .ref(ref));
                       }});
    }});
    sync(f);
    id = f.services.qrcode().mounted().front();
    const auto before = f.services.qrcode().snapshot(id);
    const auto input = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(input.editor);
    require(rich_editor.focus(), "rich QR status could not focus its retained Input");
    require(static_cast<bool>(editor.replace_range({0, editor.value().size()}, "a中文z")), "QR draft setup failed");
    require(rich_editor.select(7, 1), "QR reverse selection setup failed");
    require(static_cast<bool>(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "QR composition setup failed");
    const auto draft = std::string(editor.value());
    const auto selection = editor.selection();
    const auto history = editor.history();
    language.set(locales::zh_CN);
    sync(f);
    require(starts == 1 && cancels == 0 && renders == 1 && has_text(f, id, u8"二维码已过期") &&
                f.services.qrcode().snapshot(id).status_content == before.status_content &&
                f.services.qrcode().snapshot(id).encoding_revision == before.encoding_revision &&
                f.inputs.mounted_inputs().front().editor == input.editor && refreshes == 0,
            "QR locale update reencoded data, reset logo/renderer/editor, or fired refresh");
    require(editor.value() == draft && editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && f.inputs.focused(input.component),
            "QR locale switch lost draft, reverse selection, composition, undo or focus");
    require(completions[0]({pixels(), {}}), "QR logo completion after locale change was rejected");
    sync(f);
    require(loads == 1 && ref.refresh() && refreshes == 1, "live logo/refresh callbacks failed");
    language.set(locales::en_US);
    mode = 1;
    language.set(locales::zh_CN);
    sync(f);
    require(language.get() == locales::en_US && has_text(f, id, u8"QR code expired") && renders == 1 &&
                f.services.qrcode().snapshot(id).encoding_revision == before.encoding_revision,
            "reentrant QR locale publication overwrote the latest language");
    icon.set(ImageSource{});
    icon.set(ImageSource{loader});
    sync(f);
    require(starts == 2 && !completions[0]({pixels(), {}}), "QR logo replacement kept an old completion live");
    mode = 2;
    language.set(locales::zh_CN);
    require(!ref.bound() && !rich_editor.bound() && !f.services.components().contains(id) && cancels == 1 &&
                !completions[1]({pixels(), {}}),
            "QR locale observer retirement accessed stale State or accepted a late logo");
    static_cast<void>(f.frames.consume_request());
    const auto requests = f.frames.counters().requests;
    language.set(locales::en_US);
    require(f.frames.counters().requests == requests && !f.frames.pending(), "retired QR kept a locale observer");
    clean(f);
}

void append_during_another_scope_mount() {
    Fixture f;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<std::shared_ptr<const QRCodeStatusRenderer>> renderer{std::shared_ptr<const QRCodeStatusRenderer>{}};
    Signal<std::shared_ptr<const ImageActionsRenderer>> image_renderer{std::shared_ptr<const ImageActionsRenderer>{}};
    Signal<UploadFiles> files{UploadFiles{}};
    I18n captured;
    I18n captured_image;
    I18n captured_upload;
    int renders = 0;
    auto replacement = std::make_shared<QRCodeStatusRenderer>();
    replacement->render = [&](const QRCodeStatusContext&) {
        ++renders;
        captured = useI18n();
        return QRCodeStatusContent{[&] { Text(TextProps{}.content(captured.text(callsite))); }};
    };
    auto image_replacement = std::make_shared<ImageActionsRenderer>();
    image_replacement->render = [&](const ImagePreviewActions&) {
        captured_image = useI18n();
        Text(TextProps{}.content(captured_image.text(callsite)));
    };
    auto upload_renderer = std::make_shared<UploadItemRenderer>();
    upload_renderer->render = [&](Signal<UploadFile>, const UploadItemActions&) {
        captured_upload = useI18n();
        Text(TextProps{}.content(captured_upload.text(callsite)));
    };
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).messages(messages()), ConfigProviderContent{[&] {
                           QRCode(QRCodeProps{}.value(u8"APPEND").status(QRCodeStatus::Expired).statusRender(renderer));
                           Image(ImageProps{}.source(ImageSource{pixels()}).actionsRender(image_renderer));
                           Upload(UploadProps{}.fileList(files).itemRender(
                               std::shared_ptr<const UploadItemRenderer>{upload_renderer}));
                       }});
        ConfigProvider(ConfigProviderProps{}.locale(locales::en_US), ConfigProviderContent{[&] {
                           renderer.set(replacement);
                           f.services.qrcode().prepare_auxiliary_layout();
                           image_renderer.set(image_replacement);
                           files.set(UploadFiles{UploadFile{.uid = u8"late", .name = u8"late file"}});
                       }});
    }});
    sync(f);
    const auto id = f.services.qrcode().mounted().front();
    require(renders == 1 && captured.locale().effective == locales::zh_CN && has_text(f, id, u8"中文调用处"),
            "media append borrowed another active declaration's config context");
    require(captured_image.locale().effective == locales::zh_CN && captured_upload.locale().effective == locales::zh_CN,
            "Image actions or Upload row append borrowed a foreign declaration's config");
    language.set(locales::en_US);
    sync(f);
    require(renders == 1 && has_text(f, id, u8"English callsite"),
            "appended media status retained a snapshot instead of its live owner scope");
    clean(f);
    require(!captured.active(), "appended media slot retained a live context after retirement");
}

void equal_and_unrelated_catalog_updates_stay_idle() {
    Fixture f;
    const LocaleId custom("en-XX");
    auto data = std::make_unique<LocaleData>(LocaleCatalog::builtin().pack(locales::en_US).data());
    Signal<LocaleCatalog> catalog{LocaleCatalog({{custom, LocalePack(*data)}})};
    int mounts = 0;
    f.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(custom).catalog(catalog), ConfigProviderContent{[&] {
                           ++mounts;
                           Image(ImageProps{}.preview(false));
                           Upload();
                           QRCode(QRCodeProps{}.value(u8"IDLE").status(QRCodeStatus::Expired));
                       }});
    }});
    sync(f);
    static_cast<void>(f.frames.consume_request());
    const auto requests = f.frames.counters().requests;
    const auto encoding = f.services.qrcode().snapshot(f.services.qrcode().mounted().front()).encoding_revision;
    catalog.set(LocaleCatalog({{custom, LocalePack(*data)}}));
    // Image owns a retained Modal, so common.confirm is a used subtree dependency.
    data->picker.now = u8"unused picker action";
    catalog.set(LocaleCatalog({{custom, LocalePack(std::move(*data))}}));
    require(!f.frames.pending() && f.frames.counters().requests == requests && mounts == 1 &&
                f.services.qrcode().snapshot(f.services.qrcode().mounted().front()).encoding_revision == encoding &&
                !f.services.next_frame_deadline(),
            "equal/unrelated Locale groups dirtied media text, scheduled a deadline or reencoded QR");
    clean(f);
}

} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        scope_nesting_and_windows();
        explicit_english_empty_and_slots();
        image_loading_preview_and_identity();
        preview_group_count_and_controlled_state();
        compound_preview_locale_precedence();
        upload_async_rows_and_literal_errors();
        upload_errors_controlled_and_open_preview();
        qrcode_logo_renderer_reentry_and_retirement();
        append_during_another_scope_mount();
        equal_and_unrelated_catalog_updates_stay_idle();
        std::cout << "i18n media: 10 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
