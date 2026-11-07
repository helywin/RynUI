#include "component/config_defaults.hpp"
#include "component/upload_component.hpp"
#include "component/upload_model.hpp"
#include "component/form_focus_context.hpp"
#include "component/form_control_binding.hpp"
#include "component/image_component.hpp"
#include "component/button_component.hpp"
#include "component/localized_prop.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/icon.hpp>
#include <ryn/progress.hpp>
#include <ryn/tooltip.hpp>
#include <algorithm>
#include <cmath>
#include <thread>
#include <limits>

namespace ryn::detail {
struct UploadRefState final {
    std::function<bool()> bound;
    std::function<bool()> focus;
    std::function<bool()> open;
    std::function<bool(NativeFiles)> select;
    std::function<bool(const UploadUid&)> retry;
    std::function<bool(const UploadUid&)> abort;
    std::uint64_t binding{};
    std::thread::id owner{std::this_thread::get_id()};
    std::function<bool(const FormFocusContext&)> form_focus;
    std::function<std::optional<FormControlGeometry>(const FormFocusContext&)> form_geometry;
};

struct UploadComponentHost::Lifetime final {
    UploadComponentHost* host{};
    std::thread::id owner{std::this_thread::get_id()};
};

namespace {
thread_local UploadComponentHost* active_upload{};
constexpr float infinity = std::numeric_limits<float>::infinity();

String action_label(const UploadLocale& locale, std::size_t index) {
    switch (index) {
    case 0:
        return locale.preview;
    case 1:
        return locale.download;
    case 2:
        return locale.remove;
    case 3:
        return locale.retry;
    default:
        return locale.cancel;
    }
}

struct SelectionTicket final {
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t generation{};
    bool active{true};
    bool complete{};
    FileServiceError result{};
};

struct Row final {
    UploadRowSnapshot view;
    Signal<UploadFile> file{UploadFile{}};
    Signal<ImageSource> thumbnail{ImageSource{}};
    Signal<bool> picture{false};
    Signal<bool> card{false};
    Signal<bool> readonly{false};
    Signal<bool> disabled{false};
    Signal<LogicalLength> side{dp(64)};
    Signal<LayoutGap> gap{LayoutGap{dp(8)}};
    Signal<FlexDirection> direction{FlexDirection::LeftToRight};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<ImageShape> shape{ImageShape::Rectangle};
    Signal<UploadListType> list_type{UploadListType::Text};
    Signal<UploadLocale> locale{UploadLocale{}};
    Signal<runtime::SemanticForeground> color{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId surface;
    std::shared_ptr<const NativeFile> origin;
    ImageSource explicit_thumbnail;
    std::shared_ptr<const UploadItemRenderer> renderer;
    std::shared_ptr<const UploadIconRenderer> icon_renderer;
    std::array<runtime::ComponentId, 5> action_regions;
};

struct UploadState final {
    runtime::NodeId node;
    UploadSnapshot view;
    UploadModel model;
    bool ready{};
    bool mounting{true};
    bool refreshing{};
    bool reconciling{};
    bool reconcile_again{};
    bool visible{true};
    bool disabled{};
    bool read_only{};
    bool rtl{};
    bool multiple{};
    bool directory{};
    bool pastable{};
    bool dialog_on_click{true};
    bool geometry_available{true};
    bool hovering{};
    UploadAccept accept;
    std::optional<std::size_t> max_count;
    UploadListType list_type{UploadListType::Text};
    UploadType type{UploadType::Select};
    InputStatus status{InputStatus::Default};
    UploadListActions list_actions;
    UploadLocale locale;
    bool explicit_locale{};
    std::uint64_t locale_revision{};
    std::shared_ptr<const UploadItemRenderer> renderer;
    std::shared_ptr<const UploadIconRenderer> icon_renderer;
    UploadSlots slots;
    std::vector<std::shared_ptr<Row>> rows;
    runtime::ComponentId list;
    component::RetainedSurfaceId surface;
    runtime::SceneFragmentId drop_fragment;
    input::InteractionId drop_interaction;
    ButtonRef trigger_ref;
    Signal<bool> trigger_disabled{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<ButtonType> button_type{ButtonType::Default};
    Signal<LogicalLength> trigger_height{auto_length};
    Signal<String> trigger_label{String{u8"Select files"}};
    Signal<IconName> trigger_icon{IconName::UploadOutlined};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<std::vector<ImagePreviewItem>> preview_items{std::vector<ImagePreviewItem>{}};
    Signal<bool> preview_open{false};
    Signal<bool> preview_rtl{false};
    Signal<ImageLocale> preview_locale{ImageLocale{}};
    Signal<std::size_t> preview_current{0};
    std::function<void(const UploadFile&)> on_preview;
    std::function<void(const UploadFile&)> on_download;
    std::function<void(const NativeFiles&)> on_drop;
    std::function<void(FileServiceError, String)> on_selection_error;
    std::function<ImageSource(const UploadFile&)> preview_file;
    std::function<bool(const UploadFile&)> is_image;
    std::shared_ptr<SelectionTicket> selection;
    FileServiceCancel cancel_selection;
    std::uint64_t generation{};
    std::array<theme_runtime::Subscription, 5> subscriptions;
};

void cancel(FileServiceCancel fn) noexcept {
    if (fn) {
        try {
            fn();
        } catch (...) {
        }
    }
}

void append(WindowComponentServices& s, bool, runtime::ComponentId parent, const Content& content) {
    runtime::ComponentBuildContext* build{};
    try {
        build = &runtime::require_component_build_context();
    } catch (const std::logic_error&) {
        // A normal retained update has no active declaration context.
    }
    if (build && &build->host() == &s.components()) {
        // Another retained owner can publish while the enclosing tree still mounts.
        // Use that mount transaction, with the target parent's actual theme.
        build->mount_slot_with_theme_scope(
            Content{[&] {
                runtime::require_component_build_context().mount_slot_with_config_scope(
                    Content{[&] { runtime::require_component_build_context().mount_slot(parent, content); }},
                    s.components().config_scope(parent));
            }},
            s.components().theme_scope(parent));
    } else {
        s.append_slot(parent, content);
    }
}

runtime::ComponentId region(WindowComponentServices& s) {
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<int>(0);
    const auto node = b.root(id);
    s.layout().set_layout(node, layout::BoxLayout{{}, true});
    b.on_resource_cleanup(id, [&s, node, id] {
        s.interactions().set_input_blocked(id, false);
        s.layout().remove_layout(node);
    });
    return id;
}

runtime::Rect translated(const runtime::Node& n) {
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool inside(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

bool image_file(const UploadFile& f) {
    const auto type = f.type.bytes();
    if (type.starts_with("image/")) {
        return true;
    }
    const auto n = f.name.bytes();
    return n.ends_with(".png") || n.ends_with(".jpg") || n.ends_with(".jpeg") || n.ends_with(".bmp");
}

std::shared_ptr<Row> row(UploadState& s, const UploadUid& uid) {
    const auto it = std::ranges::find_if(s.rows, [&](const auto& r) { return r->view.uid == uid; });
    return it == s.rows.end() ? nullptr : *it;
}

String label(const UploadFile& file, const UploadLocale& l) {
    String status;
    switch (file.status) {
    case UploadFileStatus::Ready:
        status = l.ready;
        break;
    case UploadFileStatus::Uploading:
        status = l.uploading;
        break;
    case UploadFileStatus::Done:
        status = l.done;
        break;
    case UploadFileStatus::Error:
        status = file.error_source == UploadErrorSource::Builtin
                     ? l.error
                     : (file.error_source == UploadErrorSource::Literal || !file.error.empty() ? file.error : l.error);
        break;
    case UploadFileStatus::Cancelled:
        status = l.cancelled;
        break;
    case UploadFileStatus::Removed:
        status = l.remove;
        break;
    }
    auto text = std::string(file.name.bytes()) + " — " + std::string(status.bytes());
    if (file.status == UploadFileStatus::Uploading) {
        text += " " + std::to_string(static_cast<int>(file.percent)) + "%";
    }
    return String::from_utf8(text).value();
}
} // namespace

UploadComponentHost::UploadComponentHost(WindowComponentServices& s)
    : services_(&s), lifetime_(std::make_shared<Lifetime>()) {
    lifetime_->host = this;
    s.attach(*this);
}

UploadComponentHost::~UploadComponentHost() {
    lifetime_->host = nullptr;
    services_->detach(*this);
}

void* UploadComponentHost::begin_mount() noexcept {
    auto* old = active_upload;
    active_upload = this;
    return old;
}

void UploadComponentHost::end_mount(void* old) noexcept {
    active_upload = static_cast<UploadComponentHost*>(old);
}

void UploadComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    if (drop_target_ && !services_->components().contains(*drop_target_)) {
        drop_target_.reset();
        drop_paths_.clear();
    }
}

void UploadComponentHost::on_dispose() noexcept {
    mounted_.clear();
    drop_target_.reset();
    drop_paths_.clear();
}

UploadSnapshot UploadComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        throw std::out_of_range("Stale Upload component");
    }
    auto view = s->view;
    view.files = s->model.files();
    view.pending = s->model.pending_count();
    for (const auto& r : s->rows) {
        view.rows.push_back(r->view);
    }
    return view;
}

bool UploadComponentHost::editable(runtime::ComponentId id) const {
    const auto* s = services_->components().state<UploadState>(id);
    return s && active_ && s->visible && !s->disabled && !s->read_only && s->geometry_available &&
           services_->interactions().component_active(id);
}

void UploadComponentHost::cancel_selection(runtime::ComponentId id) noexcept {
    auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    ++s->generation;
    if (s->selection) {
        s->selection->active = false;
    }
    s->selection.reset();
    s->view.selecting = false;
    auto fn = std::move(s->cancel_selection);
    cancel(std::move(fn));
}

bool UploadComponentHost::select(runtime::ComponentId id, NativeFiles files) {
    if (!editable(id)) {
        return false;
    }
    auto* s = services_->components().state<UploadState>(id);
    try {
        return s->model.select(std::move(files));
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

bool UploadComponentHost::retry(runtime::ComponentId id, const UploadUid& uid) {
    if (!editable(id)) {
        return false;
    }
    try {
        return services_->components().state<UploadState>(id)->model.retry(uid);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

bool UploadComponentHost::abort(runtime::ComponentId id, const UploadUid& uid) {
    if (!editable(id)) {
        return false;
    }
    try {
        return services_->components().state<UploadState>(id)->model.abort(uid);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

bool UploadComponentHost::remove(runtime::ComponentId id, const UploadUid& uid) {
    if (!editable(id)) {
        return false;
    }
    try {
        return services_->components().state<UploadState>(id)->model.remove(uid);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

bool UploadComponentHost::choose(runtime::ComponentId id) {
    if (!editable(id)) {
        return false;
    }
    auto* s = services_->components().state<UploadState>(id);
    auto* files = services_->file_services();
    if (!files) {
        s->view.selection_error = String{u8"Native file service is unavailable"};
        const auto callback = s->on_selection_error;
        if (callback) {
            try {
                callback(FileServiceError::Unavailable, s->view.selection_error);
            } catch (...) {
                services_->retire_failed_component(id);
                throw;
            }
        }
        return false;
    }
    cancel_selection(id);
    s = services_->components().state<UploadState>(id);
    if (!s) {
        return false;
    }
    auto ticket = std::make_shared<SelectionTicket>();
    ticket->generation = s->generation;
    s->selection = ticket;
    s->view.selecting = true;
    FileDialogOptions options;
    options.multiple = s->multiple;
    options.directory = s->directory;
    for (const auto& rule : s->accept) {
        options.extensions.insert(options.extensions.end(), rule.extensions.begin(), rule.extensions.end());
    }
    const std::weak_ptr<Lifetime> weak = lifetime_;
    try {
        auto cancel = files->choose(options, [weak, id, ticket](FileSelection result) {
            if (ticket->owner != std::this_thread::get_id()) {
                return false;
            }
            const auto life = weak.lock();
            if (!life || !life->host || !ticket->active || ticket->complete) {
                return false;
            }
            auto& host = *life->host;
            auto* s = host.services_->components().state<UploadState>(id);
            if (!s || s->generation != ticket->generation || !host.editable(id)) {
                return false;
            }
            ticket->complete = true;
            ticket->result = result.error;
            s->view.selecting = false;
            s->cancel_selection = {};
            s->view.selection_error = result.message;
            const auto callback = s->on_selection_error;
            if (result.error == FileServiceError::None) {
                host.select(id, std::move(result.files));
            } else if (result.error != FileServiceError::Cancelled && callback) {
                try {
                    callback(result.error, result.message);
                } catch (...) {
                    host.services_->retire_failed_component(id);
                    throw;
                }
            }
            return true;
        });
        s = services_->components().state<UploadState>(id);
        if (s && s->generation == ticket->generation && ticket->active && !ticket->complete) {
            s->cancel_selection = std::move(cancel);
        } else if (!ticket->active) {
            detail::cancel(std::move(cancel));
        }
    } catch (...) {
        cancel_selection(id);
        throw;
    }
    return true;
}

void UploadComponentHost::resolve_drop(runtime::ComponentId id, std::vector<String> paths) {
    if (!editable(id) || paths.empty()) {
        return;
    }
    auto* service = services_->file_services();
    if (!service) {
        return;
    }
    cancel_selection(id);
    auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    auto ticket = std::make_shared<SelectionTicket>();
    ticket->generation = s->generation;
    s->selection = ticket;
    s->view.selecting = true;
    const auto directory = s->directory;
    const std::weak_ptr<Lifetime> weak = lifetime_;
    auto cancel = service->resolve(paths, directory, [weak, id, ticket](FileSelection result) {
        if (ticket->owner != std::this_thread::get_id()) {
            return false;
        }
        const auto life = weak.lock();
        if (!life || !life->host || !ticket->active || ticket->complete) {
            return false;
        }
        auto& host = *life->host;
        auto* s = host.services_->components().state<UploadState>(id);
        if (!s || s->generation != ticket->generation || !host.editable(id)) {
            return false;
        }
        ticket->complete = true;
        s->view.selecting = false;
        s->cancel_selection = {};
        const auto on_drop = s->on_drop;
        const auto error = s->on_selection_error;
        if (result.error == FileServiceError::None) {
            const auto files = result.files;
            if (on_drop) {
                try {
                    on_drop(files);
                } catch (...) {
                    host.services_->retire_failed_component(id);
                    throw;
                }
            }
            host.select(id, files);
        } else if (error && result.error != FileServiceError::Cancelled) {
            try {
                error(result.error, result.message);
            } catch (...) {
                host.services_->retire_failed_component(id);
                throw;
            }
        }
        return true;
    });
    s = services_->components().state<UploadState>(id);
    if (s && ticket->active && !ticket->complete) {
        s->cancel_selection = std::move(cancel);
    } else if (!ticket->active) {
        detail::cancel(std::move(cancel));
    }
}

bool UploadComponentHost::preview(runtime::ComponentId id, const UploadUid& uid) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s || !active_ || s->disabled || !s->visible) {
        return false;
    }
    const auto files = s->model.files();
    const auto it = std::ranges::find_if(files, [&](const auto& f) { return f.uid == uid; });
    if (it == files.end()) {
        return false;
    }
    const auto callback = s->on_preview;
    if (callback) {
        try {
            callback(*it);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        return true;
    }
    const auto items = s->preview_items.get();
    const auto item = std::ranges::find_if(items, [&](const auto& v) { return v.key == uid; });
    if (item == items.end()) {
        return false;
    }
    s->preview_current.set(item - items.begin());
    s->preview_open.set(true);
    return true;
}

bool UploadComponentHost::download(runtime::ComponentId id, const UploadUid& uid) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s || s->disabled || !s->visible || !active_ || !s->on_download) {
        return false;
    }
    const auto files = s->model.files();
    const auto it = std::ranges::find_if(files, [&](const auto& f) { return f.uid == uid; });
    if (it == files.end()) {
        return false;
    }
    const auto callback = s->on_download;
    try {
        callback(*it);
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    return true;
}

void UploadComponentHost::replace_content(runtime::ComponentId id, const UploadUid& uid) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    const auto r = row(*s, uid);
    if (!r) {
        return;
    }
    if (r->view.content.valid() && r->renderer == s->renderer && r->icon_renderer == s->icon_renderer) {
        return;
    }
    const auto old_view = r->view;
    const auto old_regions = r->action_regions;
    const auto old = r->view.content;
    r->view.image = {};
    r->view.actions = {};
    r->action_regions = {};
    const auto renderer = s->renderer;
    const auto icon_renderer = s->icon_renderer;
    const auto slots = s->slots;
    runtime::ComponentId replacement;
    const std::weak_ptr<Lifetime> weak = lifetime_;
    const auto invoke = [weak, id, uid](auto operation) {
        const auto life = weak.lock();
        if (life && life->owner == std::this_thread::get_id() && life->host) {
            (life->host->*operation)(id, uid);
        }
    };
    const UploadItemActions actions{
        [invoke] { invoke(&UploadComponentHost::preview); }, [invoke] { invoke(&UploadComponentHost::download); },
        [invoke] { invoke(&UploadComponentHost::remove); }, [invoke] { invoke(&UploadComponentHost::retry); },
        [invoke] { invoke(&UploadComponentHost::abort); }};
    try {
        append(*services_, s->mounting, r->view.component, Content{[&, r, renderer, icon_renderer, slots] {
            FormPrivateEditorScope private_editor;
            auto& b = runtime::require_component_build_context();
            replacement = region(*services_);
            b.mount_slot_with_semantic_text_style(
                replacement, Content{[&, r, renderer, icon_renderer, slots] {
                    Theme(
                        ThemeProps{}.config(bind([base = r->theme, card = r->card] {
                            auto config = base.get();
                            if (card.get()) {
                                config.button.tokens.padding_inline = dp(0);
                                config.button.tokens.padding_inline_small = dp(0);
                                config.button.tokens.padding_inline_large = dp(0);
                                config.button.tokens.icon_gap = dp(0);
                            }
                            return config;
                        })),
                        ThemeContent{[&, r, renderer, icon_renderer, slots] {
                            if (renderer && renderer->render) {
                                renderer->render(r->file, actions);
                                return;
                            }
                            Flex(FlexProps{}.vertical(r->card).direction(r->direction).gap(r->gap),
                                 FlexContent{[&, r, icon_renderer, slots] {
                                     const auto before = services_->image().mounted().size();
                                     auto image = ImageProps{}
                                                      .source(r->thumbnail)
                                                      .alt(bind([file = r->file] { return file.get().name; }))
                                                      .visible(r->picture)
                                                      .disabled(r->disabled)
                                                      .shape(r->shape)
                                                      .fit(ImageFit::Cover)
                                                      .preview(false)
                                                      .layout(LayoutStyle{}.width(r->side).height(r->side));
                                     if (s->explicit_locale) {
                                         image.locale(s->preview_locale);
                                     }
                                     Image(std::move(image));
                                     if (services_->image().mounted().size() == before + 1) {
                                         r->view.image = services_->image().mounted().back();
                                     }
                                     if (icon_renderer && icon_renderer->render) {
                                         icon_renderer->render(r->file, r->list_type);
                                     } else {
                                         Icon(IconProps{}
                                                  .name(IconName::FileOutlined)
                                                  .visible(bind([picture = r->picture] { return !picture.get(); })));
                                     }
                                     Flex(FlexProps{}.vertical(true).gap(dp(2)).layout(LayoutStyle{}.flex_grow(1)),
                                          FlexContent{[r] {
                                              Text(TextProps{}.content(bind([file = r->file, locale = r->locale] {
                                                  return label(file.get(), locale.get());
                                              })));
                                              Progress(
                                                  ProgressProps{}
                                                      .percent(bind([file = r->file] { return file.get().percent; }))
                                                      .showInfo(false)
                                                      .size(ControlSize::Small));
                                          }});
                                     Flex(FlexProps{}.wrap(true).direction(r->direction).gap(r->gap),
                                          FlexContent{[&, r, slots] {
                                              const std::array icons{IconName::EyeOutlined, IconName::DownloadOutlined,
                                                                     IconName::DeleteOutlined, IconName::ReloadOutlined,
                                                                     IconName::CloseCircleOutlined};
                                              const std::array custom{slots.preview_icon, slots.download_icon,
                                                                      slots.remove_icon, slots.retry_icon,
                                                                      slots.cancel_icon};
                                              const std::array callbacks{actions.preview, actions.download,
                                                                         actions.remove, actions.retry, actions.cancel};
                                              for (std::size_t index = 0; index < 5; ++index) {
                                                  const auto part = region(*services_);
                                                  r->action_regions[index] = part;
                                                  b.mount_slot(
                                                      part, Content{[&, r, index] {
                                                          const auto before =
                                                              services_->interactions().declaration_order().size();
                                                          Tooltip(
                                                              TooltipProps{}
                                                                  .title(bind([locale = r->locale, index] {
                                                                      return action_label(locale.get(), index);
                                                                  }))
                                                                  .disabled(
                                                                      bind([card = r->card] { return !card.get(); })),
                                                              TooltipTrigger{[&, r, index] {
                                                                  Button(ButtonProps{}
                                                                             .type(ButtonType::Text)
                                                                             .size(r->size)
                                                                             .layout(LayoutStyle{}.min_width(dp(24)))
                                                                             .disabled(index < 2 ? r->disabled
                                                                                                 : r->readonly)
                                                                             .onClick(callbacks[index]),
                                                                         ButtonContent{[r, index] {
                                                                             Text(TextProps{}.content(
                                                                                 bind([locale = r->locale,
                                                                                       card = r->card, index] {
                                                                                     return card.get()
                                                                                                ? String{}
                                                                                                : action_label(
                                                                                                      locale.get(),
                                                                                                      index);
                                                                                 })));
                                                                         }},
                                                                         ButtonIcon{[&, index] {
                                                                             if (custom[index]) {
                                                                                 SlotContentAccess::function (
                                                                                     *custom[index])();
                                                                             } else {
                                                                                 Icon(IconProps{}.name(icons[index]));
                                                                             }
                                                                         }});
                                                              }});
                                                          const auto entries =
                                                              services_->interactions().declaration_order();
                                                          for (std::size_t next = before; next < entries.size();
                                                               ++next) {
                                                              const auto& record =
                                                                  services_->interactions().require(entries[next]);
                                                              if (record.focusable) {
                                                                  r->view.actions[index] = record.component;
                                                                  break;
                                                              }
                                                          }
                                                      }});
                                              }
                                          }});
                                 }});
                        }});
                }},
                r->color, r->font);
        }});
    } catch (...) {
        r->view = old_view;
        r->action_regions = old_regions;
        if (replacement.valid()) {
            services_->retire_failed_component(replacement);
        }
        throw;
    }
    s = services_->components().state<UploadState>(id);
    if (!s) {
        if (replacement.valid()) {
            services_->retire_failed_component(replacement);
        }
        return;
    }
    r->view.content = replacement;
    r->renderer = renderer;
    r->icon_renderer = icon_renderer;
    if (old.valid()) {
        services_->destroy(old);
    }
    services_->mark_scene_structure_dirty();
}

void UploadComponentHost::build_row(runtime::ComponentId id, const UploadFile& file) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    auto r = std::make_shared<Row>();
    r->theme = s->theme;
    r->view.uid = file.uid;
    r->file.set(file);
    runtime::ComponentId component;
    append(*services_, s->mounting, id, Content{[&, r] {
        auto& b = runtime::require_component_build_context();
        component = region(*services_);
        r->view.component = component;
        const auto fragment = b.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
        r->surface = services_->surfaces().create_content_range(fragment, {});
        b.on_resource_cleanup(component, [this, r] { services_->surfaces().destroy_content_range(r->surface); });
    }});
    s = services_->components().state<UploadState>(id);
    if (!s) {
        if (component.valid()) {
            services_->retire_failed_component(component);
        }
        return;
    }
    s->rows.push_back(r);
    try {
        replace_content(id, file.uid);
    } catch (...) {
        services_->retire_failed_component(component);
        s = services_->components().state<UploadState>(id);
        if (s) {
            std::erase(s->rows, r);
        }
        throw;
    }
}

void UploadComponentHost::reconcile(runtime::ComponentId id) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s || !s->ready) {
        return;
    }
    if (s->reconciling) {
        s->reconcile_again = true;
        return;
    }
    s->reconciling = true;
    try {
        do {
            s->reconcile_again = false;
            const auto files = s->model.files();
            std::vector<std::shared_ptr<Row>> next;
            next.reserve(files.size());
            for (const auto& file : files) {
                s = services_->components().state<UploadState>(id);
                if (!s) {
                    return;
                }
                auto r = row(*s, file.uid);
                if (!r) {
                    build_row(id, file);
                    s = services_->components().state<UploadState>(id);
                    if (!s) {
                        return;
                    }
                    r = row(*s, file.uid);
                }
                if (!r) {
                    continue;
                }
                const auto predicate = s->is_image;
                const auto picture = predicate ? predicate(file) : image_file(file);
                if (!services_->components().contains(id)) {
                    return;
                }
                if (r->origin != file.origin || r->explicit_thumbnail != file.thumbnail) {
                    ImageSource image = file.thumbnail;
                    if (image.value.index() == 0 && picture) {
                        if (s->preview_file) {
                            const auto fn = s->preview_file;
                            image = fn(file);
                        } else if (file.origin && file.origin->bytes) {
                            image = ImageSource{EncodedImage::copy(*file.origin->bytes)};
                        }
                    }
                    if (!services_->components().contains(id)) {
                        return;
                    }
                    r->thumbnail.set(image);
                    r->origin = file.origin;
                    r->explicit_thumbnail = file.thumbnail;
                }
                r->file.set(file);
                if (!services_->components().contains(id)) {
                    return;
                }
                replace_content(id, file.uid);
                if (!services_->components().contains(id)) {
                    return;
                }
                next.push_back(r);
            }
            s = services_->components().state<UploadState>(id);
            if (!s) {
                return;
            }
            const auto old = s->rows;
            s->rows = std::move(next);
            for (const auto& r : old) {
                if (std::ranges::none_of(s->rows, [&](const auto& value) { return r == value; })) {
                    services_->destroy(r->view.component);
                    s = services_->components().state<UploadState>(id);
                    if (!s) {
                        return;
                    }
                }
            }
            std::vector<ImagePreviewItem> previews;
            for (const auto& r : s->rows) {
                if (r->thumbnail.get().value.index() != 0) {
                    previews.push_back({r->view.uid, r->thumbnail.get(), r->file.get().name});
                }
            }
            const auto previous = s->preview_items.get();
            const auto old_index = s->preview_current.get();
            const auto selected = old_index < previous.size() ? previous[old_index].key : String{};
            const auto match = std::ranges::find_if(previews, [&](const auto& item) { return item.key == selected; });
            const auto next_index = match != previews.end()
                                        ? static_cast<std::size_t>(match - previews.begin())
                                        : std::min(old_index, previews.empty() ? std::size_t{} : previews.size() - 1);
            batch([&] {
                s->preview_current.set(next_index);
                s->preview_items.set(std::move(previews));
            });
            s = services_->components().state<UploadState>(id);
            if (!s) {
                return;
            }
            if (s->preview_items.get().empty()) {
                s->preview_open.set(false);
            }
            s->reconciling = false;
            refresh(id, true);
            s = services_->components().state<UploadState>(id);
            if (!s) {
                return;
            }
            s->reconciling = true;
        } while (s->reconcile_again);
        s->reconciling = false;
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
}

void UploadComponentHost::update_locale(runtime::ComponentId id, UploadLocale locale) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    const auto revision = ++s->locale_revision;
    s->locale = locale;
    const auto trigger = s->trigger_label;
    const auto preview = s->preview_locale;
    const auto rows = s->rows;
    const auto text = s->type == UploadType::Drag ? locale.drop : locale.select;
    const auto emit = [this, id, revision](auto signal, auto value) {
        signal.set(std::move(value));
        const auto* current = services_->components().state<UploadState>(id);
        return current && current->locale_revision == revision;
    };
    if (!emit(trigger, text) || !emit(preview, locale.image_preview)) {
        return;
    }
    for (const auto& r : rows) {
        if (!emit(r->locale, locale)) {
            return;
        }
    }
}

void UploadComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<UploadState>(id);
    if (!s || s->refreshing) {
        return;
    }
    s->refreshing = true;
    const auto& t = services_->components().theme_scope(id)->snapshot().upload();
    services_->components().set_branch_active(id, s->visible);
    services_->nodes().require(s->node).out_of_flow = !s->visible;
    const bool enabled = editable(id);
    if (!enabled && drop_target_ == id) {
        s->view.dragging = false;
        drop_target_.reset();
        drop_paths_.clear();
    }
    services_->interactions().set_eligible(s->drop_interaction, enabled);
    const bool own_popup = services_->interactions().subtree_intersects_input_domain(id);
    const bool work_enabled =
        enabled || (own_popup && active_ && s->visible && !s->disabled && !s->read_only && s->geometry_available);
    s->trigger_disabled.set(!enabled);
    s->button_type.set(s->type == UploadType::Drag ? ButtonType::Dashed : ButtonType::Default);
    s->trigger_icon.set(s->type == UploadType::Drag ? IconName::InboxOutlined : IconName::UploadOutlined);
    s->trigger_label.set(s->type == UploadType::Drag ? s->locale.drop : s->locale.select);
    s->preview_rtl.set(s->rtl);
    s->preview_locale.set(s->locale.image_preview);
    s->trigger_height.set(s->type == UploadType::Drag ? dp(t.metrics.placeholder_size) : auto_length);
    ThemeConfig theme;
    theme.button.tokens.default_border_color =
        s->view.dragging                    ? t.colors.focus
        : s->status == InputStatus::Error   ? t.colors.error
        : s->status == InputStatus::Warning ? services_->components().theme_scope(id)->snapshot().map().color_warning
                                            : t.colors.border;
    theme.text.tokens.font_family = t.typography.font_family;
    theme.text.tokens.font_size = dp(t.typography.font_size);
    theme.text.tokens.line_height = dp(t.typography.line_height);
    theme.button.tokens.default_background = s->view.dragging ? t.colors.hover : t.colors.background;
    theme.button.tokens.default_color = t.colors.text;
    theme.button.tokens.disabled_background = t.colors.hover;
    theme.alias.color_text_disabled = t.colors.disabled;
    theme.button.tokens.text_hover_background = t.colors.hover;
    theme.button.tokens.text_color = t.colors.secondary;
    theme.button.tokens.text_hover_color = t.colors.text;
    theme.alias.color_focus_outline = t.colors.focus;
    theme.alias.line_width_focus = dp(t.effects.focus_width);
    theme.alias.focus_outline_offset = dp(t.effects.focus_offset);
    theme.seed.control_height = dp(t.metrics.row_height);
    theme.image.tokens.background = t.colors.background;
    theme.image.tokens.radius = dp(t.metrics.radius);
    theme.image.tokens.preview_width = dp(t.metrics.preview_width);
    theme.image.tokens.preview_height = dp(t.metrics.preview_height);
    theme.image.tokens.mask = t.colors.mask;
    theme.image.tokens.toolbar = t.colors.toolbar;
    theme.image.tokens.preview_text = t.colors.preview_text;
    theme.image.tokens.order = t.order;
    s->theme.set(std::move(theme));
    const auto rows = s->rows;
    for (const auto& r : rows) {
        r->card.set(s->list_type == UploadListType::PictureCard || s->list_type == UploadListType::PictureCircle);
        r->picture.set(s->list_type != UploadListType::Text && r->thumbnail.get().value.index() != 0);
        r->shape.set(s->list_type == UploadListType::PictureCircle ? ImageShape::Circle : ImageShape::Rectangle);
        r->side.set(
            dp(s->list_type == UploadListType::Picture ? t.metrics.thumbnail_size * 2 / 3 : t.metrics.thumbnail_size));
        r->gap.set(LayoutGap{dp(t.metrics.gap)});
        r->direction.set(s->rtl ? FlexDirection::RightToLeft : FlexDirection::LeftToRight);
        r->size.set(s->size.get());
        r->list_type.set(s->list_type);
        r->locale.set(s->locale);
        r->readonly.set(!enabled);
        r->disabled.set(s->disabled || !active_);
        const auto foreground = s->disabled                                       ? t.colors.disabled
                                : r->file.get().status == UploadFileStatus::Error ? t.colors.error
                                                                                  : t.colors.text;
        r->color.set({foreground.red(), foreground.green(), foreground.blue(), foreground.alpha()});
        r->font.set({t.typography.font_family, 400, false, t.typography.font_size, t.typography.line_height});
        const float padding = r->card.get() ? t.metrics.padding : 0.F;
        services_->layout().set_layout(services_->components().root(r->view.component),
                                       layout::BoxLayout{{padding, padding, padding, padding}, true});
        if (r->view.content.valid()) {
            services_->set_input_blocked(r->view.content,
                                         s->disabled || !s->visible || !active_ || !s->geometry_available);
            s = services_->components().state<UploadState>(id);
            if (!s) {
                return;
            }
        }
        const auto file = r->file.get();
        const std::array show{s->list_actions.preview && (s->on_preview || r->thumbnail.get().value.index() != 0),
                              s->list_actions.download && bool(s->on_download), s->list_actions.remove,
                              s->list_actions.retry && file.status != UploadFileStatus::Uploading &&
                                  file.status != UploadFileStatus::Done,
                              file.status == UploadFileStatus::Uploading};
        for (std::size_t i = 0; i < 5; ++i) {
            const auto part = r->action_regions[i];
            if (part.valid()) {
                services_->components().set_branch_active(part, show[i]);
                services_->nodes().require(services_->components().root(part)).out_of_flow = !show[i];
            }
        }
        services_->components().set_branch_active(r->view.component, s->list_actions.show_list);
        services_->nodes().require(services_->components().root(r->view.component)).out_of_flow =
            !s->list_actions.show_list;
    }
    if (!enabled) {
        cancel_selection(id);
    }
    s = services_->components().state<UploadState>(id);
    if (!s) {
        return;
    }
    s->model.set_enabled(work_enabled);
    if (!services_->components().contains(id)) {
        return;
    }
    if (geometry) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    services_->mark_scene_structure_dirty();
    services_->request_frame();
    s = services_->components().state<UploadState>(id);
    if (s) {
        s->refreshing = false;
    }
}

runtime::Size UploadComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    const auto& s = *services_->components().state<UploadState>(id);
    const auto& t = services_->components().theme_scope(id)->snapshot().upload();
    auto size = e.measure_child(services_->components().root(s.view.trigger), {0, c.max_width, 0, c.max_height});
    const bool card = s.list_type == UploadListType::PictureCard || s.list_type == UploadListType::PictureCircle;
    float width{};
    float height{};
    float line_width{};
    float line_height{};
    for (const auto& row : s.rows) {
        if (!s.list_actions.show_list) {
            break;
        }
        const auto v = e.measure_child(
            services_->components().root(row->view.component),
            {0, card ? std::min(c.max_width, t.metrics.thumbnail_size + 2 * t.metrics.padding) : c.max_width, 0,
             infinity});
        if (card) {
            if (line_width > 0 && line_width + t.metrics.gap + v.width > c.max_width) {
                width = std::max(width, line_width);
                height += line_height + t.metrics.gap;
                line_width = 0;
                line_height = 0;
            }
            if (line_width > 0) {
                line_width += t.metrics.gap;
            }
            line_width += v.width;
            line_height = std::max(line_height, v.height);
        } else {
            width = std::max(width, v.width);
            height += v.height + t.metrics.gap;
        }
    }
    if (!s.rows.empty() && s.list_actions.show_list) {
        if (card) {
            width = std::max(width, line_width);
            height += line_height;
        } else {
            height = std::max(0.F, height - t.metrics.gap);
        }
        size.width = std::max(size.width, std::min(width, c.max_width));
        size.height += t.metrics.gap + height;
    }
    return c.constrain(size);
}

void UploadComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect rect) {
    auto& s = *services_->components().state<UploadState>(id);
    const auto& t = services_->components().theme_scope(id)->snapshot().upload();
    const auto trigger = services_->components().root(s.view.trigger);
    const auto z = services_->nodes().require(trigger).layout_size;
    e.place_child(trigger, {rect.x, rect.y, rect.width, z.height});
    float x = s.rtl ? rect.x + rect.width : rect.x;
    float y = rect.y + z.height + t.metrics.gap;
    float line_height{};
    const bool card = s.list_type == UploadListType::PictureCard || s.list_type == UploadListType::PictureCircle;
    for (const auto& row : s.rows) {
        if (!s.list_actions.show_list) {
            break;
        }
        const auto n = services_->components().root(row->view.component);
        const auto size = services_->nodes().require(n).layout_size;
        if (card && ((!s.rtl && x + size.width > rect.x + rect.width && x > rect.x) ||
                     (s.rtl && x - size.width < rect.x && x < rect.x + rect.width))) {
            x = s.rtl ? rect.x + rect.width : rect.x;
            y += line_height + t.metrics.gap;
            line_height = 0;
        }
        if (s.rtl && card) {
            x -= size.width;
        }
        e.place_child(n, {card ? x : rect.x, y, card ? size.width : rect.width, size.height});
        if (card) {
            line_height = std::max(line_height, size.height);
            x += s.rtl ? -t.metrics.gap : size.width + t.metrics.gap;
        } else {
            y += line_height + t.metrics.gap;
            line_height = 0;
        }
    }
}

void UploadComponentHost::mount(const UploadProps& p, const UploadSlots& slots) {
    FormPrivateEditorScope private_editor;
    if (p.files_ && !p.default_files_.empty()) {
        throw std::invalid_argument("Upload fileList and defaultFileList conflict");
    }
    UploadModel::validate_files(p.files_ ? read_prop(*p.files_) : p.default_files_);
    auto& b = runtime::require_component_build_context();
    const auto locale = localized_prop(p.locale_, b.config_scope(), &LocaleData::upload);
    const auto id = b.mount_component<UploadState>();
    auto& s = b.state<UploadState>(id);
    s.node = b.root(id);
    s.slots = slots;
    s.visible = read_prop(p.visible_);
    s.disabled = read_prop(p.disabled_);
    s.read_only = read_prop(p.read_only_);
    s.rtl = read_prop(p.rtl_);
    s.multiple = read_prop(p.multiple_);
    s.directory = read_prop(p.directory_);
    s.pastable = read_prop(p.pastable_);
    s.accept = read_prop(p.accept_);
    s.max_count = read_prop(p.max_count_);
    s.list_type = read_prop(p.list_type_);
    s.type = read_prop(p.type_);
    s.status = read_prop(p.status_);
    s.explicit_locale = p.locale_.has_value();
    s.locale = read_prop(locale);
    s.renderer = read_prop(p.renderer_);
    s.icon_renderer = read_prop(p.icon_renderer_);
    s.on_preview = p.on_preview_;
    s.on_download = p.on_download_;
    s.on_drop = p.on_drop_;
    s.on_selection_error = p.on_selection_error_;
    s.preview_file = p.preview_file_;
    s.is_image = p.is_image_;
    s.dialog_on_click = read_prop(p.dialog_on_click_);
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services_->nodes(), services_->dirty());
    s.drop_interaction = services_->interactions().create({id, s.node, {}, true, false, {}, false, false});
    s.drop_fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    const auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.surface = services_->surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id, [this, id, node = s.node, surface = s.surface] {
        cancel_selection(id);
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->model.retire();
            services_->interactions().remove(s->drop_interaction);
        }
        services_->surfaces().destroy_content_range(surface);
        services_->layout().remove_layout(node);
    });
    const std::weak_ptr<Lifetime> weak = lifetime_;
    UploadModelOptions options;
    options.controlled = p.files_.has_value();
    options.enabled = !s.disabled && !s.read_only && s.visible;
    options.multiple = s.multiple;
    options.directory = s.directory;
    options.accept = s.accept;
    options.max_count = s.max_count;
    const auto guard_hook = [weak, id](UploadBefore hook) -> UploadBefore {
        if (!hook) {
            return {};
        }
        return [weak, id, hook = std::move(hook)](const UploadFile& file, const UploadFiles& files,
                                                  UploadBeforeCompletion done) -> UploadCancel {
            const auto retire = [weak, id] {
                const auto life = weak.lock();
                if (life && life->owner == std::this_thread::get_id() && life->host) {
                    life->host->services_->retire_failed_component(id);
                }
            };
            try {
                return hook(file, files, [done = std::move(done), retire](UploadBeforeResult result) {
                    try {
                        return done(std::move(result));
                    } catch (...) {
                        retire();
                        throw;
                    }
                });
            } catch (...) {
                retire();
                throw;
            }
        };
    };
    options.before = guard_hook(p.before_);
    options.transform = guard_hook(p.transform_);
    options.transport = p.transport_;
    options.remove = p.remove_;
    options.on_change = [weak, id, fn = p.on_change_](const UploadChange& change) {
        if (fn) {
            try {
                fn(change);
            } catch (...) {
                const auto life = weak.lock();
                if (life && life->host) {
                    life->host->services_->retire_failed_component(id);
                }
                throw;
            }
        }
    };
    options.changed = [weak, id] {
        const auto life = weak.lock();
        if (life && life->host) {
            life->host->reconcile(id);
        }
    };
    s.model.configure(std::move(options));
    s.model.set_files(p.files_ ? read_prop(*p.files_) : p.default_files_);
    b.mount_slot(id, Content{[&] {
                     Theme(ThemeProps{}.config(s.theme), ThemeContent{[&] {
                               const auto before = services_->interactions().declaration_order().size();
                               Button(ButtonProps{}
                                          .ref(s.trigger_ref)
                                          .disabled(s.trigger_disabled)
                                          .size(s.size)
                                          .type(s.button_type)
                                          .block(true)
                                          .layout(LayoutStyle{}.height(s.trigger_height))
                                          .onClick([this, id] {
                                              const auto* s = services_->components().state<UploadState>(id);
                                              if (s && s->dialog_on_click) {
                                                  choose(id);
                                              }
                                          }),
                                      ButtonContent{[&] {
                                          if (slots.trigger) {
                                              SlotContentAccess::function (*slots.trigger)();
                                          } else {
                                              Text(TextProps{}.content(s.trigger_label));
                                          }
                                      }},
                                      ButtonIcon{[&] { Icon(IconProps{}.name(s.trigger_icon)); }});
                               (void)before;
                           }});
                 }});
    // Trigger is the only ButtonRef target; find its exact retained component under this owner.
    for (const auto hit : services_->interactions().declaration_order()) {
        const auto& record = services_->interactions().require(hit);
        for (auto ancestor = std::optional{record.component}; ancestor;
             ancestor = services_->components().parent(*ancestor)) {
            if (*ancestor == id && record.focusable) {
                s.view.trigger = record.component;
                break;
            }
        }
        if (s.view.trigger.valid()) {
            break;
        }
    }
    b.mount_slot(id, Content{[&] {
                     Theme(ThemeProps{}.config(s.theme), ThemeContent{[&] {
                               const auto before = services_->image().mounted().size();
                               auto preview =
                                   PreviewGroupProps{}
                                       .items(s.preview_items)
                                       .thumbnails(false)
                                       .rtl(s.preview_rtl)
                                       .open(s.preview_open)
                                       .current(s.preview_current)
                                       .onOpenChange(
                                           [open = s.preview_open](bool value, std::size_t) { open.set(value); })
                                       .onChange([current = s.preview_current](std::size_t value, std::size_t) {
                                           current.set(value);
                                       });
                               if (s.explicit_locale) {
                                   preview.locale(s.preview_locale);
                               }
                               PreviewGroup(std::move(preview));
                               if (services_->image().mounted().size() > before) {
                                   s.view.preview_group = services_->image().mounted()[before];
                               }
                           }});
                 }});
    services_->layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    if (p.ref_) {
        const auto ref = p.ref_->state_;
        if (ref->bound && ref->bound()) {
            throw std::invalid_argument("UploadRef already bound");
        }
        const auto generation = ++ref->binding;
        ref->bound = [weak, id] {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host &&
                   life->host->services_->components().contains(id);
        };
        ref->form_geometry = [weak, id](const FormFocusContext& context) -> std::optional<FormControlGeometry> {
            const auto life = weak.lock();
            if (!life || life->owner != std::this_thread::get_id() || !life->host) {
                return {};
            }
            const auto* state = life->host->services_->components().state<UploadState>(id);
            return state ? context.geometry(*life->host->services_, state->view.trigger) : std::nullopt;
        };
        ref->form_focus = [weak, id](const FormFocusContext& context) {
            const auto life = weak.lock();
            if (!life || life->owner != std::this_thread::get_id() || !life->host ||
                !context.visible(*life->host->services_, id)) {
                return false;
            }
            const auto* s = life->host->services_->components().state<UploadState>(id);
            return s && context.visible(*life->host->services_, s->view.trigger) && s->trigger_ref.focus();
        };
        ref->focus = [weak, id] {
            const auto life = weak.lock();
            if (!life || life->owner != std::this_thread::get_id() || !life->host) {
                return false;
            }
            const auto* s = life->host->services_->components().state<UploadState>(id);
            return s && s->trigger_ref.focus();
        };
        ref->open = [weak, id] {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host && life->host->choose(id);
        };
        ref->select = [weak, id](NativeFiles f) {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host &&
                   life->host->select(id, std::move(f));
        };
        ref->retry = [weak, id](const UploadUid& u) {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host && life->host->retry(id, u);
        };
        ref->abort = [weak, id](const UploadUid& u) {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host && life->host->abort(id, u);
        };
        b.on_resource_cleanup(id, [ref, generation] {
            if (ref->binding == generation) {
                ref->bound = {};
                ref->focus = {};
                ref->open = {};
                ref->select = {};
                ref->retry = {};
                ref->abort = {};
                ref->form_focus = {};
                ref->form_geometry = {};
            }
        });
    }
    auto& scope = b.scope(id);
    const auto refresh_options = [this, id] {
        auto* s = services_->components().state<UploadState>(id);
        if (s) {
            s->model.set_options(s->accept, s->multiple, s->directory, s->max_count);
            refresh(id);
        }
    };
    connect_prop(scope, p.accept_, [this, id, refresh_options](UploadAccept v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->accept = std::move(v);
            refresh_options();
        }
    });
    connect_prop(scope, p.multiple_, [this, id, refresh_options](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->multiple = v;
            refresh_options();
        }
    });
    connect_prop(scope, p.directory_, [this, id, refresh_options](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->directory = v;
            refresh_options();
        }
    });
    connect_prop(scope, p.max_count_, [this, id, refresh_options](std::optional<std::size_t> v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->max_count = v;
            refresh_options();
        }
    });
    connect_prop(scope, p.pastable_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->pastable = v;
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->visible = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->disabled = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.read_only_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->read_only = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->rtl = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.size_, [this, id](ControlSize v) {
        if (v < ControlSize::Small || v > ControlSize::Large) {
            throw std::invalid_argument("Invalid Upload size");
        }
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->size.set(v);
            refresh(id);
        }
    });
    connect_prop(scope, p.status_, [this, id](InputStatus v) {
        if (v != InputStatus::Default && v != InputStatus::Warning && v != InputStatus::Error) {
            throw std::invalid_argument("Invalid Upload status");
        }
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->status = v;
            refresh(id, false);
        }
    });
    connect_prop(scope, p.list_type_, [this, id](UploadListType v) {
        if (v < UploadListType::Text || v > UploadListType::PictureCircle) {
            throw std::invalid_argument("Invalid Upload list type");
        }
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->list_type = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.type_, [this, id](UploadType v) {
        if (v < UploadType::Select || v > UploadType::Drag) {
            throw std::invalid_argument("Invalid Upload type");
        }
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->type = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.list_actions_, [this, id](UploadListActions v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->list_actions = v;
            refresh(id);
        }
    });
    connect_prop(scope, locale, [this, id](UploadLocale v) { update_locale(id, std::move(v)); });
    connect_prop(scope, p.dialog_on_click_, [this, id](bool v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->dialog_on_click = v;
        }
    });
    connect_prop(scope, p.renderer_, [this, id](std::shared_ptr<const UploadItemRenderer> v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->renderer = std::move(v);
            if (s->ready) {
                const auto files = s->model.files();
                try {
                    for (const auto& file : files) {
                        replace_content(id, file.uid);
                    }
                    refresh(id);
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
        }
    });
    connect_prop(scope, p.icon_renderer_, [this, id](std::shared_ptr<const UploadIconRenderer> v) {
        if (auto* s = services_->components().state<UploadState>(id)) {
            s->icon_renderer = std::move(v);
            if (s->ready) {
                const auto files = s->model.files();
                try {
                    for (const auto& file : files) {
                        replace_content(id, file.uid);
                    }
                    refresh(id);
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
        }
    });
    if (p.files_) {
        connect_prop(scope, *p.files_, [this, id](UploadFiles v) {
            if (auto* s = services_->components().state<UploadState>(id)) {
                s->model.set_files(std::move(v));
            }
        });
    }
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::upload_colors, &theme_runtime::ThemeScope::upload_metrics,
                               &theme_runtime::ThemeScope::upload_typography,
                               &theme_runtime::ThemeScope::upload_effects, &theme_runtime::ThemeScope::upload_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, i](theme_runtime::DirtyPhase) { refresh(id, i != 0); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    s.ready = true;
    reconcile(id);
    if (auto* live = services_->components().state<UploadState>(id)) {
        live->mounting = false;
        refresh(id);
    }
}

void UploadComponentHost::on_window_active(bool value) {
    active_ = value;
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id);
    }
    if (!value) {
        drop_target_.reset();
        drop_paths_.clear();
    }
}

void UploadComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id, false);
    }
    if (drop_target_ && !editable(*drop_target_)) {
        drop_target_.reset();
        drop_paths_.clear();
    }
}

void UploadComponentHost::prepare_auxiliary_layout() {
    if (auto* files = services_->file_services()) {
        files->drain();
    }
}

void UploadComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<UploadState>(id);
        if (!s) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().upload();
        const auto r = translated(services_->nodes().require(s->node));
        s->view.bounds = r;
        const bool available = r.width > 0 && r.height > 0 && services_->nodes().content_opacity(s->node) > 0 &&
                               services_->components().branch_active(id);
        if (s->geometry_available != available) {
            s->geometry_available = available;
            refresh(id, false);
            s = services_->components().state<UploadState>(id);
            if (!s) {
                continue;
            }
        }
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        s->view.clip = clip;
        services_->scene_composer().set_fragment(s->drop_fragment, {}, s->drop_interaction, clip);
        std::vector<graphics::RoundedEffectInstance> effects;
        for (const auto& row : s->rows) {
            const auto bounds =
                translated(services_->nodes().require(services_->components().root(row->view.component)));
            row->view.bounds = bounds;
            std::vector<graphics::RoundedEffectInstance> visual;
            if (available && s->list_actions.show_list &&
                (s->list_type == UploadListType::PictureCard || s->list_type == UploadListType::PictureCircle)) {
                auto shape = graphics::LogicalRoundedRect{
                    bounds, std::min(t.metrics.radius, std::min(bounds.width, bounds.height) / 2)};
                if (s->list_type == UploadListType::PictureCircle) {
                    shape.rect.width = std::min(bounds.width, t.metrics.thumbnail_size + 2 * t.metrics.padding);
                    shape.rect.height = shape.rect.width;
                    shape.radius = shape.rect.width / 2;
                }
                const auto fills = graphics::make_corner_fill_effects(
                    shape, {true, true, true, true}, t.colors.background, 1, {}, graphics::EffectClip{1, clip});
                visual.assign(fills.begin(), fills.end());
                const auto outlines = graphics::make_corner_outline_effects(
                    shape, {true, true, true, true}, t.metrics.border_width, 0,
                    s->status == InputStatus::Error ? t.colors.error : t.colors.border, 1, {},
                    graphics::EffectClip{1, clip});
                visual.insert(visual.end(), outlines.begin(), outlines.end());
            }
            services_->surfaces().update_content_effects(row->surface, visual);
        }
        services_->surfaces().update_content_effects(s->surface, effects);
    }
}

bool UploadComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.key != input::Key::v || event.action != input::KeyAction::down || event.repeat ||
        !input::has_modifier(event.modifiers, event.primary_modifier)) {
        return false;
    }
    const auto focus = services_->focus().state().focused;
    if (!focus) {
        return false;
    }
    const auto* record = services_->interactions().find(*focus);
    if (!record || record->focus_handlers && record->focus_handlers->text_edit) {
        return false;
    }
    for (auto ancestor = std::optional{record->component}; ancestor;
         ancestor = services_->components().parent(*ancestor)) {
        auto* s = services_->components().state<UploadState>(*ancestor);
        if (!s || !s->pastable || !editable(*ancestor)) {
            continue;
        }
        auto* service = services_->file_services();
        if (!service) {
            return false;
        }
        const auto id = *ancestor;
        cancel_selection(id);
        s = services_->components().state<UploadState>(id);
        if (!s) {
            return true;
        }
        auto ticket = std::make_shared<SelectionTicket>();
        ticket->generation = s->generation;
        s->selection = ticket;
        s->view.selecting = true;
        const std::weak_ptr<Lifetime> weak = lifetime_;
        auto fn = service->paste([weak, id, ticket](FileSelection result) {
            if (ticket->owner != std::this_thread::get_id()) {
                return false;
            }
            const auto life = weak.lock();
            if (!life || !life->host || !ticket->active || ticket->complete) {
                return false;
            }
            auto& host = *life->host;
            auto* s = host.services_->components().state<UploadState>(id);
            if (!s || s->generation != ticket->generation || !host.editable(id)) {
                return false;
            }
            ticket->complete = true;
            ticket->result = result.error;
            s->view.selecting = false;
            s->cancel_selection = {};
            if (result.error == FileServiceError::None) {
                host.select(id, std::move(result.files));
            }
            return true;
        });
        s = services_->components().state<UploadState>(id);
        if (s && ticket->active && !ticket->complete) {
            s->cancel_selection = std::move(fn);
        } else if (!ticket->active) {
            detail::cancel(std::move(fn));
        }
        return !ticket->complete || ticket->result != FileServiceError::Unavailable;
    }
    return false;
}

bool UploadComponentHost::on_file_drop(const input::FileDropEvent& event) {
    if (event.window_id != services_->file_window_id() || !active_) {
        return false;
    }
    const auto target_at = [this](float x, float y) -> std::optional<runtime::ComponentId> {
        const auto hit = services_->hit_test().hit_test({x, y});
        if (!hit) {
            return {};
        }
        for (auto ancestor = std::optional{services_->interactions().require(*hit).component}; ancestor;
             ancestor = services_->components().parent(*ancestor)) {
            const auto* s = services_->components().state<UploadState>(*ancestor);
            if (s && editable(*ancestor) && inside(s->view.bounds, x, y) && inside(s->view.clip, x, y)) {
                return *ancestor;
            }
        }
        return {};
    };
    const auto set_target = [this](std::optional<runtime::ComponentId> target) {
        const auto old = drop_target_;
        if (old == target) {
            return;
        }
        drop_target_ = target;
        drop_paths_.clear();
        for (auto id : {old, target}) {
            if (id) {
                if (auto* s = services_->components().state<UploadState>(*id)) {
                    s->view.dragging = target == id;
                    refresh(*id, false);
                }
            }
        }
    };
    if (event.action == input::FileDropAction::Begin) {
        set_target({});
        drop_paths_.clear();
        return false;
    }
    if (event.action == input::FileDropAction::Position || event.action == input::FileDropAction::File) {
        const auto target = target_at(event.x, event.y);
        set_target(target);
        drop_position_ = {event.x, event.y};
        if (!target) {
            return false;
        }
        if (event.action == input::FileDropAction::File) {
            if (drop_paths_.size() >= 4096) {
                set_target({});
                return false;
            }
            drop_paths_.push_back(event.locator);
        }
        return true;
    }
    if (event.action == input::FileDropAction::Complete && drop_target_) {
        const auto target = *drop_target_;
        auto paths = std::move(drop_paths_);
        const bool valid = target_at(drop_position_.x, drop_position_.y) == drop_target_;
        set_target({});
        if (valid) {
            resolve_drop(target, std::move(paths));
            return true;
        }
    }
    return false;
}

std::function<bool(const FormFocusContext&)> capture_form_upload(const UploadRef& ref) {
    const auto binding = ref.state_->binding;
    const std::weak_ptr<UploadRefState> weak = ref.state_;
    return [weak, binding](const FormFocusContext& context) {
        const auto s = weak.lock();
        if (!s || s->owner != std::this_thread::get_id() || s->binding != binding) {
            return false;
        }
        const auto focus = s->form_focus;
        return focus && focus(context);
    };
}

std::function<std::optional<FormControlGeometry>(const FormFocusContext&)>
capture_form_upload_geometry(const UploadRef& ref) {
    const auto binding = ref.state_->binding;
    const std::weak_ptr<UploadRefState> weak = ref.state_;
    return [weak, binding](const FormFocusContext& context) -> std::optional<FormControlGeometry> {
        const auto state = weak.lock();
        if (!state || state->owner != std::this_thread::get_id() || state->binding != binding) {
            return {};
        }
        const auto geometry = state->form_geometry;
        return geometry ? geometry(context) : std::nullopt;
    };
}

} // namespace ryn::detail

namespace ryn {
UploadRef::UploadRef() : state_(std::make_shared<detail::UploadRefState>()) {}

bool UploadRef::bound() const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->bound;
    return f && f();
}

bool UploadRef::focus() const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->focus;
    return f && f();
}

bool UploadRef::open() const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->open;
    return f && f();
}

bool UploadRef::select(NativeFiles v) const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->select;
    return f && f(std::move(v));
}

bool UploadRef::retry(const UploadUid& v) const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->retry;
    return f && f(v);
}

bool UploadRef::abort(const UploadUid& v) const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->abort;
    return f && f(v);
}

void Upload(UploadProps props, UploadSlots slots) {
    detail::ConfigDefaults::apply(props);
    if (!detail::active_upload) {
        throw std::logic_error("Upload requires WindowComponentServices");
    }
    detail::active_upload->mount(props, slots);
}
} // namespace ryn
