#include "component/config_defaults.hpp"
#include "component/image_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/image_model.hpp"
#include "component/localized_prop.hpp"
#include "component/modal_component.hpp"
#include "component/popover_component.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/icon.hpp>
#include <ryn/modal.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <thread>

namespace ryn::detail {
struct ImageRefState final {
    std::function<bool()> bound;
    std::function<bool()> focus;
    std::function<bool(bool)> open;
    std::function<bool(ImageAction)> action;
    std::uint64_t binding{};
    std::thread::id owner{std::this_thread::get_id()};
};

struct ImageComponentHost::Lifetime final {
    ImageComponentHost* host{};
    std::thread::id owner{std::this_thread::get_id()};
};

namespace {
thread_local ImageComponentHost* active_image{};
constexpr float infinity = std::numeric_limits<float>::infinity();

struct ImageTicket final {
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t generation{};
    bool active{true};
    bool completed{};
};

struct Thumbnail final {
    ImagePreviewItem item;
    runtime::ComponentId component;
    Signal<ImageSource> source{ImageSource{}};
    Signal<String> alt{String{}};
};

struct ImageState final {
    runtime::NodeId node;
    ImageSnapshot view;
    ImageSource source;
    ImageSource fallback;
    ImageSource preview_source;
    Signal<ImageSource> fallback_signal{ImageSource{}};
    std::shared_ptr<const ImagePixels> preview_pixels;
    std::shared_ptr<ImageTicket> preview_ticket;
    ImageCancel preview_cancel;
    std::uint64_t preview_generation{};
    ImageShape shape{};
    ImageFit fit{ImageFit::Contain};
    ImagePreviewOptions options;
    Signal<ImagePreviewOptions> options_state{ImagePreviewOptions{}};
    Signal<float> preview_width{800};
    bool geometry_available{true};
    ImageLocale locale;
    std::uint64_t locale_revision{};
    bool explicit_alt{};
    bool visible{true};
    bool disabled{};
    bool rtl{};
    bool group{};
    bool show_thumbnails{true};
    Signal<bool> thumbnails_visible{true};
    bool mounting{true};
    bool defer_load{true};
    bool ready{};
    bool needs_load{true};
    String selected_key;
    bool controlled_open{};
    bool controlled_index{};
    std::optional<Prop<std::size_t>> current_prop;
    std::uint64_t group_revision{};
    bool desired_open{};
    bool hovering{};
    bool focused{};
    bool suspended{};
    bool refreshing{};
    std::size_t desired_index{};
    std::uint64_t generation{};
    std::shared_ptr<ImageTicket> ticket;
    ImageCancel cancel;
    std::shared_ptr<const ImagePixels> pixels;
    std::optional<graphics::ImageId> preview_image;
    std::vector<graphics::ImageId> rounded_parts;
    std::optional<input::PointerIdentity> drag;
    runtime::Point last_pointer;
    runtime::Rect base_preview;
    input::PressableBehavior press;
    runtime::SceneFragmentId fragment;
    runtime::SceneFragmentId preview_fragment;
    component::RetainedSurfaceId background;
    runtime::SceneFragmentId cover_background_fragment;
    component::RetainedSurfaceId cover_background;
    component::RetainedSurfaceId mask;
    runtime::ComponentId placeholder;
    runtime::ComponentId error;
    runtime::ComponentId cover;
    runtime::ComponentId thumbnails;
    std::vector<Thumbnail> items;
    std::shared_ptr<const ImageActionsRenderer> renderer;
    Signal<bool> modal_open{false};
    Signal<ThemeConfig> preview_theme{ThemeConfig{}};
    Signal<FlexDirection> action_direction{FlexDirection::LeftToRight};
    Signal<LayoutGap> action_gap{LayoutGap{dp(8)}};
    Signal<LogicalLength> thumbnail_size{dp(96)};
    Signal<String> alt{String{}};
    Signal<String> message{String{}};
    Signal<String> cover_text{String{u8"Preview"}};
    Signal<ImageTransform> transform{ImageTransform{}};
    Signal<std::size_t> current{0};
    Signal<std::size_t> total{1};
    Signal<bool> button_disabled{false};
    Signal<ImageLocale> labels{ImageLocale{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> cover_foreground{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    std::function<void(bool)> on_open;
    std::function<void(bool, std::size_t)> on_group_open;
    std::function<void(std::size_t, std::size_t)> on_change;
    std::function<void()> on_load;
    std::function<void(const ImageError&)> on_error;
    std::function<void(ImageTransform, ImageAction)> on_transform;
    std::array<theme_runtime::Subscription, 5> subscriptions;
};

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
    s.layout().set_layout(
        node, layout::ComponentLayout{[&s](layout::LayoutEngine& e, runtime::NodeId n, layout::Constraints c) {
                                          runtime::Size size;
                                          for (auto child : s.nodes().require(n).children) {
                                              const auto v = e.measure_child(child, c);
                                              size.width = std::max(size.width, v.width);
                                              size.height = std::max(size.height, v.height);
                                          }
                                          return c.constrain(size);
                                      },
                                      [&s](layout::LayoutEngine& e, runtime::NodeId n, runtime::Rect r) {
                                          for (auto child : s.nodes().require(n).children) {
                                              const auto size = s.nodes().require(child).layout_size;
                                              e.place_child(child, {r.x + (r.width - size.width) / 2,
                                                                    r.y + (r.height - size.height) / 2, size.width,
                                                                    size.height});
                                          }
                                      }});
    b.on_resource_cleanup(id, [&s, node, id] {
        s.interactions().set_input_blocked(id, false);
        s.layout().remove_layout(node);
    });
    return id;
}

runtime::Rect translated(const runtime::Node& node) {
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

bool inside(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

bool empty_source(const ImageSource& s) {
    return s.value.index() == 0;
}

void invoke_cancel(ImageCancel cancel) noexcept {
    if (cancel) {
        try {
            cancel();
        } catch (...) {
        }
    }
}

void validate_items(const std::vector<ImagePreviewItem>& items) {
    if (items.size() > 4096) {
        throw std::length_error("PreviewGroup item limit exceeded");
    }
    std::set<std::string> keys;
    for (const auto& item : items) {
        if (item.key.empty() || !keys.insert(std::string(item.key.bytes())).second) {
            throw std::invalid_argument("PreviewGroup requires unique nonempty keys");
        }
    }
}
} // namespace

ImageComponentHost::ImageComponentHost(WindowComponentServices& s)
    : services_(&s), lifetime_(std::make_shared<Lifetime>()) {
    lifetime_->host = this;
    s.attach(*this);
}

ImageComponentHost::~ImageComponentHost() {
    lifetime_->host = nullptr;
    services_->detach(*this);
}

void* ImageComponentHost::begin_mount() noexcept {
    auto* old = active_image;
    active_image = this;
    return old;
}

void ImageComponentHost::end_mount(void* old) noexcept {
    active_image = static_cast<ImageComponentHost*>(old);
}

void ImageComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void ImageComponentHost::on_dispose() noexcept {
    mounted_.clear();
    cover_policies_.clear();
}

ImageSnapshot ImageComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        throw std::out_of_range("Stale Image component");
    }
    auto v = s->view;
    v.transform = s->transform.get();
    v.current = s->current.get();
    v.total = s->total.get();
    v.dragging = s->drag.has_value();
    for (const auto& item : s->items) {
        v.thumbnails.push_back(item.component);
    }
    return v;
}

void ImageComponentHost::cancel_preview(runtime::ComponentId id) noexcept {
    auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    ++s->preview_generation;
    if (s->preview_ticket) {
        s->preview_ticket->active = false;
    }
    s->preview_ticket.reset();
    auto cancel = std::move(s->preview_cancel);
    invoke_cancel(std::move(cancel));
}

void ImageComponentHost::load_preview(runtime::ComponentId id) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || !s->view.open || empty_source(s->preview_source) || s->preview_ticket) {
        return;
    }
    const auto source = s->preview_source;
    const auto gen = ++s->preview_generation;
    auto ticket = std::make_shared<ImageTicket>();
    ticket->generation = gen;
    s->preview_ticket = ticket;
    const std::weak_ptr<Lifetime> weak = lifetime_;
    ImageCompletion done = [weak, id, ticket](ImageDecodeResult result) {
        if (std::this_thread::get_id() != ticket->owner) {
            return false;
        }
        const auto life = weak.lock();
        if (!life || !life->host || !ticket->active || ticket->completed) {
            return false;
        }
        auto& host = *life->host;
        auto* s = host.services_->components().state<ImageState>(id);
        if (!s || s->preview_generation != ticket->generation || !s->view.open) {
            return false;
        }
        ticket->completed = true;
        s->preview_cancel = {};
        s->preview_pixels = result.pixels;
        const auto callback = s->on_error;
        const auto error = result.error;
        host.refresh(id);
        if (!result.pixels && callback) {
            try {
                callback(error);
            } catch (...) {
                host.services_->retire_failed_component(id);
                throw;
            }
        }
        return true;
    };
    try {
        if (const auto* v = std::get_if<std::shared_ptr<const ImagePixels>>(&source.value)) {
            done({*v, *v ? ImageError{} : ImageError{ImageErrorCode::empty, "Null preview pixels"}});
        } else if (const auto* v = std::get_if<std::shared_ptr<const EncodedImage>>(&source.value)) {
            done(*v ? decode_image(**v) : ImageDecodeResult{{}, {ImageErrorCode::empty, "Null preview source"}});
        } else if (const auto* v = std::get_if<std::shared_ptr<const ImageLoader>>(&source.value)) {
            if (!*v || !(*v)->start) {
                done({{}, {ImageErrorCode::loader_failure, "Preview loader unavailable"}});
                return;
            }
            auto cancel = (*v)->start(done);
            s = services_->components().state<ImageState>(id);
            if (s && s->preview_generation == gen && ticket->active && !ticket->completed) {
                s->preview_cancel = std::move(cancel);
            } else if (!ticket->completed) {
                invoke_cancel(std::move(cancel));
            }
        }
    } catch (const std::exception& e) {
        if (!ticket->completed) {
            done({{}, {ImageErrorCode::loader_failure, e.what()}});
        } else {
            throw;
        }
    } catch (...) {
        if (!ticket->completed) {
            done({{}, {ImageErrorCode::loader_failure, "Preview loader threw"}});
        } else {
            throw;
        }
    }
}

void ImageComponentHost::cancel_load(runtime::ComponentId id) noexcept {
    auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    ++s->generation;
    if (s->ticket) {
        if (!s->ticket->completed) {
            s->needs_load = true;
        }
        s->ticket->active = false;
    }
    s->ticket.reset();
    auto cancel = std::move(s->cancel);
    invoke_cancel(std::move(cancel));
}

void ImageComponentHost::load(runtime::ComponentId id, ImageSource source, bool fallback) {
    const auto* previous = services_->components().state<ImageState>(id);
    if (!previous) {
        return;
    }
    const auto expected_generation = previous->generation + 1;
    cancel_load(id);
    cancel_preview(id);
    auto* s = services_->components().state<ImageState>(id);
    if (!s || s->generation != expected_generation) {
        return;
    }
    s->preview_pixels.reset();
    s->needs_load = false;
    if (s->defer_load || !s->ready || !s->visible || s->disabled || !active_ || s->suspended ||
        !s->geometry_available) {
        s->needs_load = true;
        s->view.status = ImageStatus::Loading;
        refresh(id);
        return;
    }
    const auto gen = s->generation;
    auto ticket = std::make_shared<ImageTicket>();
    ticket->generation = gen;
    s->ticket = ticket;
    s->view.status = ImageStatus::Loading;
    s->view.error = {};
    s->pixels.reset();
    if (s->view.image) {
        services_->images().destroy(*s->view.image);
        s->view.image.reset();
    }
    for (auto part : s->rounded_parts) {
        services_->images().destroy(part);
    }
    s->rounded_parts.clear();
    if (s->preview_image) {
        services_->images().destroy(*s->preview_image);
        s->preview_image.reset();
    }
    refresh(id);
    if (!services_->components().contains(id)) {
        return;
    }
    const std::weak_ptr<Lifetime> weak = lifetime_;
    ImageCompletion completion = [weak, ticket, id, fallback](ImageDecodeResult result) {
        if (std::this_thread::get_id() != ticket->owner) {
            return false;
        }
        auto life = weak.lock();
        if (!life || !life->host || !ticket->active || ticket->completed) {
            return false;
        }
        ticket->completed = true;
        return life->host->complete(id, ticket->generation, std::move(result), fallback);
    };
    try {
        if (auto* pixels = std::get_if<std::shared_ptr<const ImagePixels>>(&source.value)) {
            completion({*pixels, *pixels ? ImageError{} : ImageError{ImageErrorCode::empty, "Null decoded image"}});
        } else if (auto* encoded = std::get_if<std::shared_ptr<const EncodedImage>>(&source.value)) {
            completion(*encoded ? decode_image(**encoded)
                                : ImageDecodeResult{{}, {ImageErrorCode::empty, "Null encoded image"}});
        } else if (auto* loader = std::get_if<std::shared_ptr<const ImageLoader>>(&source.value)) {
            if (!*loader || !(*loader)->start) {
                completion({{}, {ImageErrorCode::loader_failure, "Image loader is unavailable"}});
                return;
            }
            auto cancel = (*loader)->start(completion);
            s = services_->components().state<ImageState>(id);
            if (s && s->generation == gen && ticket->active && !ticket->completed) {
                s->cancel = std::move(cancel);
            } else if (!ticket->completed) {
                invoke_cancel(std::move(cancel));
            }
        } else {
            ticket->completed = true;
            s = services_->components().state<ImageState>(id);
            if (s) {
                s->view.status = ImageStatus::Empty;
                refresh(id);
            }
        }
    } catch (const std::exception& e) {
        if (!ticket->completed) {
            completion({{}, {ImageErrorCode::loader_failure, e.what()}});
        } else {
            throw;
        }
    } catch (...) {
        if (!ticket->completed) {
            completion({{}, {ImageErrorCode::loader_failure, "Image loader threw"}});
        } else {
            throw;
        }
    }
}

bool ImageComponentHost::complete(runtime::ComponentId id, std::uint64_t gen, ImageDecodeResult result, bool fallback) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || s->generation != gen || !s->ticket || !s->ticket->active) {
        return false;
    }
    s->cancel = {};
    if (result.pixels) {
        s->pixels = std::move(result.pixels);
        s->view.status = fallback ? ImageStatus::Fallback : ImageStatus::Ready;
        s->view.error = {};
        const auto callback = s->on_load;
        refresh(id);
        if (callback) {
            try {
                callback();
            } catch (...) {
                services_->retire_failed_component(id);
                throw;
            }
        }
        return true;
    }
    s->view.status = ImageStatus::Error;
    s->view.error = result.error ? std::move(result.error)
                                 : ImageError{ImageErrorCode::loader_failure, "Loader returned no pixels"};
    const auto error = s->view.error;
    const auto callback = s->on_error;
    refresh(id);
    if (callback) {
        try {
            callback(error);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
    s = services_->components().state<ImageState>(id);
    if (s && s->generation == gen && !fallback && !empty_source(s->fallback)) {
        const auto source = s->fallback;
        load(id, source, true);
    }
    return true;
}

void ImageComponentHost::update_locale(runtime::ComponentId id, ImageLocale locale) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    const auto revision = ++s->locale_revision;
    s->locale = locale;
    const auto labels = s->labels;
    const auto cover = s->cover_text;
    const auto message = s->message;
    const auto text = s->view.status == ImageStatus::Loading
                          ? locale.loading
                          : (s->explicit_alt || !s->view.alt.empty() ? s->view.alt : locale.error);
    const auto emit = [this, id, revision](auto signal, auto value) {
        signal.set(std::move(value));
        const auto* current = services_->components().state<ImageState>(id);
        return current && current->locale_revision == revision;
    };
    if (!emit(labels, locale) || !emit(cover, locale.preview)) {
        return;
    }
    static_cast<void>(emit(message, text));
}

void ImageComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || s->refreshing) {
        return;
    }
    s->refreshing = true;
    const auto& t = services_->components().theme_scope(id)->snapshot().image();
    ThemeConfig preview_theme;
    preview_theme.seed.control_height = dp(t.metrics.row_height);
    preview_theme.button.tokens.default_background = t.colors.toolbar;
    preview_theme.button.tokens.default_color = t.colors.preview_text;
    preview_theme.alias.color_focus_outline = t.colors.focus;
    preview_theme.alias.line_width_focus = dp(t.effects.focus_width);
    preview_theme.alias.focus_outline_offset = dp(t.effects.focus_offset);
    preview_theme.modal.tokens.background = t.colors.toolbar;
    preview_theme.modal.tokens.header_background = t.colors.toolbar;
    preview_theme.modal.tokens.text = t.colors.preview_text;
    preview_theme.modal.tokens.title = t.colors.preview_text;
    preview_theme.modal.tokens.mask = t.colors.mask;
    preview_theme.modal.tokens.close = t.colors.preview_text;
    preview_theme.modal.tokens.close_hover = t.colors.preview_text;
    preview_theme.modal.tokens.close_hover_background = t.colors.cover;
    preview_theme.modal.tokens.padding_inline = dp(t.metrics.padding);
    preview_theme.modal.tokens.padding_block = dp(t.metrics.padding);
    preview_theme.modal.tokens.font_size = dp(t.typography.font_size);
    preview_theme.modal.tokens.line_height = dp(t.typography.line_height);
    preview_theme.modal.tokens.font_family = t.typography.font_family;
    preview_theme.modal.tokens.z_index_popup =
        services_->components().theme_scope(id)->snapshot().modal().z_index_popup + t.order;
    s->preview_theme.set(std::move(preview_theme));
    s->action_direction.set(s->rtl ? FlexDirection::RightToLeft : FlexDirection::LeftToRight);
    s->action_gap.set(LayoutGap{dp(t.metrics.gap)});
    s->thumbnail_size.set(dp(t.metrics.thumbnail_size));
    const auto foreground = s->disabled                              ? t.colors.disabled
                            : s->view.status == ImageStatus::Error   ? t.colors.error
                            : s->view.status == ImageStatus::Loading ? t.colors.secondary
                                                                     : t.colors.text;
    s->foreground.set({foreground.red(), foreground.green(), foreground.blue(), foreground.alpha()});
    s->cover_foreground.set({t.colors.preview_text.red(), t.colors.preview_text.green(), t.colors.preview_text.blue(),
                             t.colors.preview_text.alpha()});
    s->typography.set({t.typography.font_family, 400, false, t.typography.font_size, t.typography.line_height});
    s->message.set(s->view.status == ImageStatus::Loading
                       ? s->locale.loading
                       : (s->explicit_alt || !s->view.alt.empty() ? s->view.alt : s->locale.error));
    s->cover_text.set(s->locale.preview);
    s->labels.set(s->locale);
    s->options_state.set(s->options);
    s->preview_width.set(t.metrics.preview_width);
    const bool ready = bool(s->pixels);
    const bool shown = s->visible && services_->components().scope(id).active();
    services_->components().set_branch_active(id, shown);
    services_->nodes().require(s->node).out_of_flow = !shown || (s->group && !s->show_thumbnails);
    if (s->group) {
        s->thumbnails_visible.set(shown && s->show_thumbnails);
    }
    for (const auto [part, on] :
         std::array{std::pair{s->placeholder, !ready && s->view.status != ImageStatus::Error},
                    std::pair{s->error, !ready && s->view.status == ImageStatus::Error},
                    std::pair{s->cover, ready && s->options.enabled && (s->hovering || s->focused) && !s->disabled}}) {
        if (part.valid() && services_->components().set_branch_active(part, on)) {
            services_->mark_scene_structure_dirty();
        }
        if (part.valid()) {
            services_->set_input_blocked(part, s->disabled || !active_ || !s->geometry_available);
            s = services_->components().state<ImageState>(id);
            if (!s) {
                return;
            }
        }
    }
    s->suspended = !services_->interactions().subtree_intersects_input_domain(id);
    const bool eligible =
        shown && !s->disabled && !s->suspended && s->geometry_available && active_ && s->options.enabled && ready;
    if (s->view.interaction.valid()) {
        services_->interactions().set_eligible(s->view.interaction, eligible);
        services_->interactions().set_focusable(s->view.interaction, eligible);
    }
    s->button_disabled.set(!ready || s->disabled);
    const bool open = s->desired_open && eligible;
    const bool changed = s->view.open != open;
    s->view.open = open;
    if (!open) {
        s->drag.reset();
        if (s->view.canvas_interaction.valid()) {
            services_->pointer().cancel_pointer_interaction(s->view.canvas_interaction);
        }
    }
    if (!open && s->preview_ticket && !s->preview_ticket->completed) {
        cancel_preview(id);
    }
    s->modal_open.set(open);
    s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    s->refreshing = false;
    if (geometry || changed) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    services_->mark_scene_structure_dirty();
    services_->request_frame();
    if (open) {
        load_preview(id);
    }
    s = services_->components().state<ImageState>(id);
    if (s && s->ready && !s->defer_load && s->needs_load && s->visible && !s->disabled && !s->suspended &&
        s->geometry_available && active_ && services_->components().branch_active(id)) {
        const auto source = s->source;
        load(id, source);
    }
}

bool ImageComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || !active_ || s->disabled || !s->visible || !s->options.enabled || s->suspended || !s->geometry_available ||
        !services_->components().branch_active(id)) {
        return false;
    }
    if (open && (!s->pixels || !services_->components().contains(s->view.modal) ||
                 !services_->components().contains(s->view.canvas))) {
        return false;
    }
    const auto callback = s->on_open;
    const auto group_callback = s->on_group_open;
    const auto current = s->current.get();
    if (!s->controlled_open) {
        s->desired_open = open;
        if (open) {
            ImageTransform t;
            t.scale = s->options.min_scale;
            s->transform.set(t);
        }
        refresh(id);
    }
    try {
        if (callback) {
            callback(open);
        }
        if (services_->components().contains(id) && group_callback) {
            group_callback(open, current);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    return true;
}

bool ImageComponentHost::action(runtime::ComponentId id, ImageAction action) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || !s->view.open || s->disabled || !active_ || !services_->components().branch_active(id) ||
        !services_->interactions().component_active(s->view.canvas)) {
        return false;
    }
    if (action == ImageAction::Close) {
        return request_open(id, false);
    }
    if (action == ImageAction::Next || action == ImageAction::Previous) {
        if (!s->group || s->items.empty()) {
            return false;
        }
        const auto i = s->current.get();
        const bool next = action == ImageAction::Next;
        if ((next && i + 1 >= s->items.size()) || (!next && i == 0)) {
            return false;
        }
        select(id, next ? i + 1 : i - 1, true);
        return true;
    }
    const auto b = s->base_preview;
    const auto viewport = s->view.canvas_bounds;
    const auto next = image_transform_action(s->transform.get(), action, {b.width, b.height},
                                             {viewport.width, viewport.height}, s->options);
    if (next == s->transform.get()) {
        return false;
    }
    const auto callback = s->on_transform;
    s->transform.set(next);
    refresh(id, false);
    if (callback) {
        try {
            callback(next, action);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
    return true;
}

void ImageComponentHost::replace_actions(runtime::ComponentId id,
                                         std::shared_ptr<const ImageActionsRenderer> renderer) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || (s->renderer == renderer && s->view.actions.valid())) {
        return;
    }
    if (!services_->components().contains(s->view.modal)) {
        s->renderer = std::move(renderer);
        return;
    }
    const auto old = s->view.actions;
    const auto modal = services_->modal().snapshot(s->view.modal).body;
    runtime::ComponentId replacement;
    const std::weak_ptr<Lifetime> weak = lifetime_;
    const auto actions = ImagePreviewActions{[weak, id](ImageAction a) {
                                                 const auto life = weak.lock();
                                                 if (life && life->owner == std::this_thread::get_id() && life->host) {
                                                     life->host->action(id, a);
                                                 }
                                             },
                                             s->transform, s->current, s->total};
    try {
        append(*services_, s->mounting, modal, Content{[&, renderer] {
            auto& b = runtime::require_component_build_context();
            replacement = region(*services_);

            b.mount_slot(
                replacement, Content{[&, renderer] {
                    FormPrivateEditorScope private_actions;
                    if (renderer && renderer->render) {
                        renderer->render(actions);
                        return;
                    }
                    const auto labels = s->labels;
                    const auto disabled = s->button_disabled;
                    const auto gap = s->action_gap;
                    const auto direction = s->action_direction;
                    const auto options = s->options_state;
                    Flex(FlexProps{}.wrap(true).gap(gap).direction(direction), FlexContent{[=] {
                             const auto button = [&](ImageAction a, IconName icon, auto member) {
                                 Button(ButtonProps{}
                                            .disabled(bind([=] {
                                                if (disabled.get()) {
                                                    return true;
                                                }
                                                const auto transform = actions.transform.get();
                                                const auto o = options.get();
                                                if (a == ImageAction::ZoomIn) {
                                                    return transform.scale >= o.max_scale;
                                                }
                                                if (a == ImageAction::ZoomOut) {
                                                    return transform.scale <= o.min_scale;
                                                }
                                                if (a == ImageAction::Previous || a == ImageAction::Next) {
                                                    const bool next = a == ImageAction::Next;
                                                    return next ? actions.current.get() + 1 >= actions.total.get()
                                                                : actions.current.get() == 0;
                                                }
                                                return false;
                                            }))
                                            .onClick([=] { actions.invoke(a); }),
                                        [=] {
                                            Icon(IconProps{}.name(bind([a, icon, direction] {
                                                const bool rtl = direction.get() == FlexDirection::RightToLeft;
                                                if (a == ImageAction::Previous) {
                                                    return rtl ? IconName::RightOutlined : IconName::LeftOutlined;
                                                }
                                                if (a == ImageAction::Next) {
                                                    return rtl ? IconName::LeftOutlined : IconName::RightOutlined;
                                                }
                                                return icon;
                                            })));
                                            Text(TextProps{}.content(bind([=] { return (labels.get().*member); })));
                                        });
                             };
                             button(ImageAction::Previous, IconName::LeftOutlined, &ImageLocale::previous);
                             button(ImageAction::Next, IconName::RightOutlined, &ImageLocale::next);
                             button(ImageAction::ZoomOut, IconName::ZoomOutOutlined, &ImageLocale::zoom_out);
                             button(ImageAction::ZoomIn, IconName::ZoomInOutlined, &ImageLocale::zoom_in);
                             button(ImageAction::RotateLeft, IconName::RotateLeftOutlined, &ImageLocale::rotate_left);
                             button(ImageAction::RotateRight, IconName::RotateRightOutlined,
                                    &ImageLocale::rotate_right);
                             button(ImageAction::FlipX, IconName::SwapOutlined, &ImageLocale::flip_x);
                             button(ImageAction::FlipY, IconName::SwapOutlined, &ImageLocale::flip_y);
                             button(ImageAction::Reset, IconName::ReloadOutlined, &ImageLocale::reset);
                             button(ImageAction::Close, IconName::CloseOutlined, &ImageLocale::close);
                             Text(TextProps{}.content(bind([=] {
                                 const auto pattern = labels.get().count;
                                 if (pattern.empty()) {
                                     return String{};
                                 }
                                 static const MessageKey key{
                                     u8"ryn.image.count",
                                     String{u8"{current} / {total}"},
                                     {{u8"current", MessageArgType::UInt}, {u8"total", MessageArgType::UInt}}};
                                 return formatMessage(key, pattern,
                                                      {{u8"current", std::uint64_t(actions.current.get() + 1)},
                                                       {u8"total", std::uint64_t(actions.total.get())}},
                                                      IntegerPluralRule::OtherOnly);
                             })));
                         }});
                }});
        }});
    } catch (...) {
        if (replacement.valid()) {
            services_->retire_failed_component(replacement);
        }
        throw;
    }
    s = services_->components().state<ImageState>(id);
    if (!s) {
        if (replacement.valid()) {
            services_->retire_failed_component(replacement);
        }
        return;
    }
    s->renderer = std::move(renderer);
    s->view.actions = replacement;
    if (old.valid()) {
        services_->destroy(old);
    }
    services_->mark_scene_structure_dirty();
}

void ImageComponentHost::mount_preview(runtime::ComponentId id) {
    auto& b = runtime::require_component_build_context();
    auto& s = b.state<ImageState>(id);
    if (!s.options.enabled || services_->components().contains(s.view.modal)) {
        return;
    }
    b.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.preview_theme), ThemeContent{[&] {
                      const auto before = services_->modal().mounted().size();
                      Modal(ModalProps{}
                                .open(s.modal_open)
                                .title(s.alt)
                                .width(s.preview_width)
                                .centered(true)
                                .focusTrap(bind([o = s.options_state] { return o.get().focus_trap; }))
                                .keyboard(bind([o = s.options_state] { return o.get().keyboard; }))
                                .mask(bind([o = s.options_state] { return ModalMask{true, o.get().mask_closable}; }))
                                .footer(ModalFooterMode::None)
                                .onCancel([this, id](ModalCancelReason) { request_open(id, false); }),
                            ModalContent{[&] {
                                auto& build = runtime::require_component_build_context();
                                s.view.canvas = region(*services_);
                                const auto node = build.root(s.view.canvas);
                                services_->nodes().require(node).clip_content = true;
                                services_->layout().set_layout(
                                    node,
                                    layout::ComponentLayout{
                                        [this, id](layout::LayoutEngine&, runtime::NodeId, layout::Constraints c) {
                                            const auto& t = services_->components().theme_scope(id)->snapshot().image();
                                            return c.constrain({t.metrics.preview_width, t.metrics.preview_height});
                                        },
                                        [](layout::LayoutEngine&, runtime::NodeId, runtime::Rect) {}});
                                s.preview_fragment = build.register_scene_fragment(
                                    s.view.canvas, runtime::SceneFragmentPlacement::before_children);
                                s.view.canvas_interaction =
                                    services_->interactions().create({s.view.canvas, node, {}, true, true});
                                services_->interactions().set_handlers(
                                    s.view.canvas_interaction,
                                    {{}, [this, id](input::PointerDispatchContext& c) { canvas_pointer(id, c); }, {}});
                                build.on_resource_cleanup(
                                    s.view.canvas, [this, interaction = s.view.canvas_interaction, node] {
                                        services_->pointer().cancel_interaction(interaction, false);
                                        services_->interactions().remove(interaction);
                                        services_->layout().remove_layout(node);
                                    });
                            }});
                      if (services_->modal().mounted().size() != before + 1) {
                          throw std::logic_error("Image preview Modal did not mount");
                      }
                      s.view.modal = services_->modal().mounted().back();
                  }});
        }});
    replace_actions(id, s.renderer);
}

void ImageComponentHost::mount(const ImageProps& p, const ImageSlots& slots) {
    validate_image_preview(read_prop(p.preview_));
    const auto fit = read_prop(p.fit_);
    if (fit < ImageFit::Contain || fit > ImageFit::ScaleDown) {
        throw std::invalid_argument("Invalid Image fit");
    }
    auto& b = runtime::require_component_build_context();
    const auto locale = localized_prop(p.locale_, b.config_scope(), &LocaleData::image);
    const auto id = b.mount_component<ImageState>();
    auto& s = b.state<ImageState>(id);
    s.node = b.root(id);
    s.source = read_prop(p.source_);
    s.fallback = read_prop(p.fallback_);
    s.preview_source = read_prop(p.preview_source_);
    s.fit = fit;
    s.shape = read_prop(p.shape_);
    s.options = read_prop(p.preview_);
    s.locale = read_prop(locale);
    s.visible = read_prop(p.visible_);
    s.disabled = read_prop(p.disabled_);
    s.rtl = read_prop(p.rtl_);
    s.explicit_alt = p.alt_.has_value() && !p.inherit_empty_alt_;
    s.view.alt = p.alt_ ? read_prop(*p.alt_) : String{};
    s.alt.set(s.view.alt);
    s.controlled_open = p.open_.has_value();
    s.desired_open = p.open_ ? read_prop(*p.open_) : p.default_open_;
    s.on_open = p.on_open_;
    s.on_load = p.on_load_;
    s.on_error = p.on_error_;
    s.on_transform = p.on_transform_;
    s.renderer = read_prop(p.actions_);
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services_->nodes(), services_->dirty());
    services_->nodes().require(s.node).clip_content = true;
    services_->nodes().require(s.node).clip_decorations = false;
    const auto background = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    const graphics::QuadInstance initial{{}, {}, 0};
    s.background = services_->surfaces().create_surface(id, s.node, background, std::span{&initial, 1});
    s.cover_background_fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    const auto mask = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.mask = services_->surfaces().create_content_range(mask, {});
    s.view.interaction = services_->interactions().create({id, s.node, {}, true, true});
    services_->interactions().set_handlers(
        s.view.interaction,
        {{},
         [this, id](input::PointerDispatchContext& context) {
             auto* state = services_->components().state<ImageState>(id);
             if (!state) {
                 return;
             }
             if (context.kind() == input::PointerEventKind::enter || context.kind() == input::PointerEventKind::leave) {
                 state->hovering = context.kind() == input::PointerEventKind::enter;
                 refresh(id, false);
             }
             state = services_->components().state<ImageState>(id);
             if (!state) {
                 return;
             }
             const auto result =
                 state->press.dispatch(context, state->view.interaction, !state->disabled && state->visible && active_);
             if (result.activate) {
                 request_open(id, true);
             }
         },
         {}});
    services_->interactions().set_focus_handlers(s.view.interaction,
                                                 {[this, id](input::FocusPresentation f) {
                                                      if (auto* state = services_->components().state<ImageState>(id)) {
                                                          state->focused = f.focus_visible;
                                                          refresh(id, false);
                                                      }
                                                  },
                                                  [this, id] {
                                                      const auto* state = services_->components().state<ImageState>(id);
                                                      return state && state->pixels && !state->disabled && active_;
                                                  },
                                                  [this, id] { request_open(id, true); },
                                                  {}});
    b.on_resource_cleanup(
        id, [this, id, node = s.node, background = s.background, mask = s.mask, interaction = s.view.interaction] {
            cancel_load(id);
            cancel_preview(id);
            auto* state = services_->components().state<ImageState>(id);
            if (state) {
                if (state->cover_background.valid()) {
                    services_->surfaces().destroy_content_range(state->cover_background);
                }
                if (state->view.image) {
                    services_->images().destroy(*state->view.image);
                }
                for (auto part : state->rounded_parts) {
                    services_->images().destroy(part);
                }
                if (state->preview_image) {
                    services_->images().destroy(*state->preview_image);
                }
            }
            services_->pointer().cancel_interaction(interaction, false);
            services_->interactions().remove(interaction);
            services_->layout().remove_layout(node);
            services_->surfaces().destroy(background);
            services_->surfaces().destroy_content_range(mask);
        });
    b.mount_slot(id, Content{[&] {
                     const auto part = [&](const auto& slot, Signal<runtime::SemanticForeground> color, bool cover) {
                         const auto part = region(*services_);
                         b.mount_slot_with_semantic_text_style(
                             part, slot ? Content{SlotContentAccess::function(*slot)} : Content{[&, cover] {
                                 Text(TextProps{}.content(cover ? s.cover_text : s.message));
                             }},
                             color, s.typography);
                         return part;
                     };
                     s.placeholder = part(slots.placeholder, s.foreground, false);
                     s.error = part(slots.error, s.foreground, false);
                     s.cover = part(slots.cover, s.cover_foreground, true);
                 }});
    if (!services_->components().contains(id)) {
        return;
    }
    mount_preview(id);
    if (!services_->components().contains(id)) {
        return;
    }
    services_->layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    if (p.ref_) {
        const auto ref = p.ref_->state_;
        if (ref->bound && ref->bound()) {
            throw std::invalid_argument("ImageRef already bound");
        }
        const auto binding = ++ref->binding;
        const std::weak_ptr<Lifetime> weak = lifetime_;
        ref->bound = [weak, id] {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host &&
                   life->host->services_->components().contains(id);
        };
        ref->focus = [weak, id] {
            const auto life = weak.lock();
            if (!life || life->owner != std::this_thread::get_id() || !life->host) {
                return false;
            }
            auto& host = *life->host;
            const auto* state = host.services_->components().state<ImageState>(id);
            return state &&
                   host.services_->focus().request_focus(state->view.interaction, input::FocusModality::keyboard);
        };
        ref->open = [weak, id](bool value) {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host &&
                   life->host->request_open(id, value);
        };
        ref->action = [weak, id](ImageAction value) {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() && life->host && life->host->action(id, value);
        };
        b.on_resource_cleanup(id, [ref, binding] {
            if (ref->binding == binding) {
                ref->bound = {};
                ref->focus = {};
                ref->open = {};
                ref->action = {};
            }
        });
    }
    auto& scope = b.scope(id);
    connect_prop(scope, p.preview_source_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            if (s->preview_source != v) {
                cancel_preview(id);
                s = services_->components().state<ImageState>(id);
                if (s) {
                    s->preview_source = std::move(v);
                    s->preview_pixels.reset();
                    refresh(id);
                }
            }
        }
    });
    connect_prop(scope, p.fallback_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->fallback = std::move(v);
        }
    });
    connect_prop(scope, p.source_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            if (s->source == v && !s->needs_load) {
                return;
            }
            s->source = v;
            s->needs_load = true;
            if (s->ready) {
                load(id, std::move(v));
            }
        }
    });
    connect_prop(scope, p.shape_, [this, id](ImageShape v) {
        if (v < ImageShape::Rectangle || v > ImageShape::RoundedRectangle) {
            throw std::invalid_argument("Invalid Image shape");
        }
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->shape = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.fit_, [this, id](ImageFit v) {
        if (v < ImageFit::Contain || v > ImageFit::ScaleDown) {
            throw std::invalid_argument("Invalid Image fit");
        }
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->fit = v;
            refresh(id);
        }
    });
    if (p.alt_) {
        connect_prop(scope, *p.alt_, [this, id](String v) {
            if (auto* s = services_->components().state<ImageState>(id)) {
                s->view.alt = v;
                s->alt.set(v);
                refresh(id);
            }
        });
    }
    connect_prop(scope, p.preview_, [this, id](ImagePreviewOptions v) {
        validate_image_preview(v);
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->options = v;
            if (v.enabled && !services_->components().contains(s->view.modal)) {
                try {
                    append(*services_, s->mounting, id, Content{[this, id] { mount_preview(id); }});
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->visible = v;
            if (!v) {
                cancel_load(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->disabled = v;
            if (v) {
                cancel_load(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->rtl = v;
            refresh(id);
        }
    });
    connect_prop(scope, locale, [this, id](ImageLocale v) { update_locale(id, std::move(v)); });
    connect_prop(scope, p.actions_,
                 [this, id](std::shared_ptr<const ImageActionsRenderer> v) { replace_actions(id, std::move(v)); });
    if (p.open_) {
        connect_prop(scope, *p.open_, [this, id](bool v) {
            if (auto* s = services_->components().state<ImageState>(id)) {
                if (v && !s->desired_open) {
                    s->transform.set({});
                }
                s->desired_open = v;
                refresh(id);
            }
        });
    }
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::image_colors, &theme_runtime::ThemeScope::image_metrics,
                               &theme_runtime::ThemeScope::image_typography, &theme_runtime::ThemeScope::image_effects,
                               &theme_runtime::ThemeScope::image_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, i](theme_runtime::DirtyPhase) { refresh(id, i != 0); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    s.mounting = false;
    s.ready = true;
    const auto source = s.source;
    load(id, source);
}

runtime::Size ImageComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& e, layout::Constraints c) {
    const auto& s = *services_->components().state<ImageState>(id);
    const auto& t = services_->components().theme_scope(id)->snapshot().image();
    runtime::Size size = s.pixels ? runtime::Size{float(s.pixels->width()), float(s.pixels->height())}
                                  : runtime::Size{t.metrics.placeholder_size, t.metrics.placeholder_size};
    if (!s.group && s.pixels) {
        const float ratio = size.width / size.height;
        if (c.min_width == c.max_width && c.min_height != c.max_height) {
            size = {c.max_width, c.max_width / ratio};
        }
        if (c.min_height == c.max_height && c.min_width != c.max_width) {
            size = {c.max_height * ratio, c.max_height};
        }
    }
    if (s.group) {
        size = {};
        if (!s.show_thumbnails) {
            return c.constrain(size);
        }
        for (const auto& item : s.items) {
            const auto v = e.measure_child(services_->components().root(item.component),
                                           {0, t.metrics.thumbnail_size, 0, t.metrics.thumbnail_size});
            size.width += v.width + t.metrics.gap;
            size.height = std::max(size.height, v.height);
        }
        if (!s.items.empty()) {
            size.width -= t.metrics.gap;
        }
    }
    for (const auto part : {s.placeholder, s.error, s.cover}) {
        if (part.valid()) {
            static_cast<void>(e.measure_child(services_->components().root(part), {0, c.max_width, 0, c.max_height}));
        }
    }
    return c.constrain(size);
}

void ImageComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect r) {
    auto& s = *services_->components().state<ImageState>(id);
    for (const auto part : {s.placeholder, s.error, s.cover}) {
        if (part.valid()) {
            e.place_child(services_->components().root(part), r);
        }
    }
    if (s.group && !s.show_thumbnails) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().image();
    float x = s.rtl ? r.x + r.width : r.x;
    for (const auto& item : s.items) {
        const auto n = services_->components().root(item.component);
        const auto z = services_->nodes().require(n).layout_size;
        if (s.rtl) {
            x -= z.width;
        }
        e.place_child(n, {x, r.y, z.width, z.height});
        x += s.rtl ? -t.metrics.gap : z.width + t.metrics.gap;
    }
}

void ImageComponentHost::canvas_pointer(runtime::ComponentId id, input::PointerDispatchContext& c) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || !s->view.open || !s->options.movable || !active_) {
        return;
    }
    const auto& event = c.event();
    if (c.kind() == input::PointerEventKind::down && event.button == input::PointerButton::primary &&
        c.capture_pointer()) {
        s->drag = event.pointer;
        s->last_pointer = {event.x, event.y};
        c.stop_propagation();
    } else if (c.kind() == input::PointerEventKind::move && s->drag == event.pointer) {
        auto transform = s->transform.get();
        transform.x += event.x - s->last_pointer.x;
        transform.y += event.y - s->last_pointer.y;
        s->last_pointer = {event.x, event.y};
        transform = clamp_image_transform(transform, {s->base_preview.width, s->base_preview.height},
                                          {s->view.canvas_bounds.width, s->view.canvas_bounds.height}, s->options);
        const auto callback = s->on_transform;
        s->transform.set(transform);
        refresh(id, false);
        c.stop_propagation();
        if (callback) {
            try {
                callback(transform, ImageAction::Drag);
            } catch (...) {
                services_->retire_failed_component(id);
                throw;
            }
        }
    } else if ((c.kind() == input::PointerEventKind::up || c.kind() == input::PointerEventKind::cancel) &&
               s->drag == event.pointer) {
        s->drag.reset();
        static_cast<void>(c.release_pointer_capture());
        c.stop_propagation();
    }
}

bool ImageComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (!active_ || event.action != input::KeyAction::down) {
        return false;
    }
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        const auto* s = services_->components().state<ImageState>(*it);
        if (!s || !s->view.open || !s->options.keyboard) {
            continue;
        }
        if (!services_->components().contains(s->view.modal) || !services_->components().contains(s->view.canvas) ||
            !services_->interactions().component_active(s->view.canvas) ||
            !services_->modal().snapshot(s->view.modal).topmost) {
            continue;
        }
        if (event.key == input::Key::escape) {
            // Modal arbitrates the innermost popup/preview after editor IME handling.
            return false;
        }
        if (event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (const auto focus = services_->focus().state().focused; focus && *focus != s->view.canvas_interaction) {
            const auto* record = services_->interactions().find(*focus);
            if (record && ((record->focus_handlers && record->focus_handlers->text_edit) ||
                           services_->popover().owns_escape(s->view.actions, record->component))) {
                return false;
            }
        }
        if (event.key == input::Key::left) {
            return action(*it, s->rtl ? ImageAction::Next : ImageAction::Previous);
        }
        if (event.key == input::Key::right) {
            return action(*it, s->rtl ? ImageAction::Previous : ImageAction::Next);
        }
        if (event.key == input::Key::up) {
            return action(*it, ImageAction::ZoomIn);
        }
        if (event.key == input::Key::down) {
            return action(*it, ImageAction::ZoomOut);
        }
        if (event.key == input::Key::home) {
            return action(*it, ImageAction::Reset);
        }
        return false;
    }
    return false;
}

bool ImageComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        const auto* s = services_->components().state<ImageState>(*it);
        if (s && s->view.open && s->options.wheel && inside(s->view.canvas_bounds, event.x, event.y) &&
            event.delta_y != 0 && services_->components().contains(s->view.modal) &&
            services_->components().contains(s->view.canvas) &&
            services_->interactions().component_active(s->view.canvas) &&
            services_->modal().snapshot(s->view.modal).topmost &&
            services_->hit_test().hit_test({event.x, event.y}) == s->view.canvas_interaction) {
            return action(*it, event.delta_y > 0 ? ImageAction::ZoomIn : ImageAction::ZoomOut);
        }
    }
    return false;
}

void ImageComponentHost::on_window_active(bool value) {
    active_ = value;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!value) {
            cancel_load(id);
        }
        refresh(id);
    }
}

void ImageComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<ImageState>(id);
        if (!s) {
            continue;
        }
        if (!services_->interactions().component_active(s->view.canvas)) {
            s->drag.reset();
        }
        if (!services_->components().branch_active(id)) {
            cancel_load(id);
            if (!s->controlled_open) {
                s->desired_open = false;
            }
        }
        refresh(id, false);
    }
}

void ImageComponentHost::set_cover_rounding(runtime::ComponentId cover, float radius) {
    std::erase_if(cover_policies_,
                  [this](const auto& policy) { return !services_->components().contains(policy.first); });
    auto found = std::ranges::find_if(cover_policies_, [cover](const auto& policy) { return policy.first == cover; });
    if (found == cover_policies_.end()) {
        cover_policies_.push_back({cover, radius});
    } else {
        found->second = radius;
    }
}

void ImageComponentHost::publish(runtime::ComponentId id, runtime::Size viewport, runtime::Rect document_clip) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().image();
    const auto& node = services_->nodes().require(s->node);
    const auto bounds = translated(node);
    s->view.bounds = bounds;
    const bool shown = services_->components().branch_active(id) && services_->nodes().content_opacity(s->node) > 0 &&
                       (s->group || (bounds.width > 0 && bounds.height > 0));
    const auto clip = services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
    if (s->geometry_available != shown) {
        s->geometry_available = shown;
        refresh(id, false);
        s = services_->components().state<ImageState>(id);
        if (!s) {
            return;
        }
    }
    const bool ready = shown && bool(s->pixels);
    if (!s->group) {
        auto visual = bounds;
        if (s->shape == ImageShape::Circle) {
            const auto side = std::min(bounds.width, bounds.height);
            visual = {bounds.x + (bounds.width - side) / 2, bounds.y + (bounds.height - side) / 2, side, side};
        }
        std::optional<float> cover_radius;
        const auto parent = services_->components().parent(id);
        if (s->shape == ImageShape::Rectangle && parent) {
            for (const auto& policy : cover_policies_) {
                if (policy.first == *parent) {
                    cover_radius = policy.second;
                    break;
                }
            }
        }
        const float radius = s->shape == ImageShape::Circle ? visual.width / 2
                                                            : std::min(cover_radius.value_or(t.metrics.radius),
                                                                       std::min(visual.width, visual.height) / 2);
        const auto opacity = services_->nodes().content_opacity(s->node);
        const graphics::QuadInstance bg{{visual.x, visual.y, visual.width, visual.height},
                                        std::array{t.colors.background.red(), t.colors.background.green(),
                                                   t.colors.background.blue(), t.colors.background.alpha()},
                                        shown && !cover_radius ? opacity : 0.F,
                                        radius};
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&bg, 1}));
        if (cover_radius && !s->cover_background.valid()) {
            s->cover_background = services_->surfaces().create_content_range(s->cover_background_fragment, {});
        }
        if (s->cover_background.valid()) {
            const auto fills = graphics::make_corner_fill_effects(
                {visual, radius}, {true, true, false, false}, t.colors.background,
                shown && cover_radius ? opacity : 0.F, {}, graphics::EffectClip{1, clip});
            services_->surfaces().update_content_effects(s->cover_background, fills);
        }
        component::RetainedSurfaceEffects fx;
        fx.shape = {visual, radius};
        fx.focus_width = t.effects.focus_width;
        fx.focus_offset = t.effects.focus_offset;
        fx.focus_color = t.colors.focus;
        fx.focus_opacity = s->focused ? opacity : 0.F;
        fx.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(services_->surfaces().update_effects(s->background, fx));
        std::vector<graphics::QuadInstance> mask;
        const bool show_mask = ready && (s->hovering || s->focused) && s->options.enabled && !s->disabled;
        if (show_mask && !cover_radius) {
            mask.push_back({{visual.x, visual.y, visual.width, visual.height},
                            std::array{t.colors.cover.red(), t.colors.cover.green(), t.colors.cover.blue(),
                                       t.colors.cover.alpha()},
                            opacity,
                            radius});
        }
        static_cast<void>(services_->surfaces().update_content_range(s->mask, mask));
        std::vector<graphics::RoundedEffectInstance> border;
        if (show_mask && cover_radius) {
            const auto fills =
                graphics::make_corner_fill_effects({visual, radius}, {true, true, false, false}, t.colors.cover,
                                                   opacity, {}, graphics::EffectClip{1, clip});
            border.insert(border.end(), fills.begin(), fills.end());
        }
        if (shown && visual.width > 0 && visual.height > 0 && t.metrics.border_width > 0) {
            const auto color = s->disabled                            ? t.colors.disabled
                               : s->view.status == ImageStatus::Error ? t.colors.error
                               : s->hovering                          ? t.colors.hover
                                                                      : t.colors.border;
            const auto outlines = graphics::make_corner_outline_effects(
                {visual, radius}, {true, true, !cover_radius, !cover_radius}, t.metrics.border_width, 0, color, opacity,
                {}, graphics::EffectClip{1, clip});
            border.insert(border.end(), outlines.begin(), outlines.end());
        }
        services_->surfaces().update_content_effects(s->mask, border);
        std::vector<graphics::SceneDrawCommand> commands;
        if (ready) {
            const auto pixels = s->pixels;
            const auto r = fit_image(bounds, {float(pixels->width()), float(pixels->height())}, s->fit);
            graphics::ImageInstance instance{pixels, r, clip, {r.x + r.width / 2, r.y + r.height / 2}};
            instance.opacity = services_->nodes().content_opacity(s->node) * services_->nodes().paint_opacity(s->node);
            if (s->shape == ImageShape::Circle && bounds.width > 0 && bounds.height > 0) {
                instance.circle_clip = visual;
            }
            const auto pieces = (cover_radius || s->shape == ImageShape::RoundedRectangle)
                                    ? graphics::decompose_rounded_image(instance, visual, cover_radius.value_or(radius),
                                                                        cover_radius.has_value())
                                    : std::vector{instance};
            if (!pieces.empty()) {
                if (!s->view.image) {
                    s->view.image = services_->images().create(pieces[0]);
                } else {
                    services_->images().update(*s->view.image, pieces[0]);
                }
                commands.push_back(
                    {graphics::SceneDrawKind::image, s->view.image->index, 1, s->view.image->generation});
            }
            while (s->rounded_parts.size() > (pieces.empty() ? 0 : pieces.size() - 1)) {
                services_->images().destroy(s->rounded_parts.back());
                s->rounded_parts.pop_back();
            }
            for (std::size_t i = 1; i < pieces.size(); ++i) {
                if (i - 1 >= s->rounded_parts.size()) {
                    s->rounded_parts.push_back(services_->images().create(pieces[i]));
                } else {
                    services_->images().update(s->rounded_parts[i - 1], pieces[i]);
                }
                const auto part = s->rounded_parts[i - 1];
                commands.push_back({graphics::SceneDrawKind::image, part.index, 1, part.generation});
            }
        }
        if (!ready) {
            for (auto part : s->rounded_parts) {
                services_->images().destroy(part);
            }
            s->rounded_parts.clear();
        }
        if (!ready && s->view.image) {
            services_->images().destroy(*s->view.image);
            s->view.image.reset();
        }
        services_->scene_composer().set_fragment(s->fragment, commands, s->view.interaction, clip);
    }
    if (services_->components().contains(s->view.canvas)) {
        const auto n = services_->components().root(s->view.canvas);
        const auto r = translated(services_->nodes().require(n));
        s->view.canvas_bounds = r;
        std::vector<graphics::SceneDrawCommand> commands;
        if (s->view.open && s->pixels && services_->components().branch_active(s->view.canvas) && r.width > 0 &&
            r.height > 0) {
            const auto pixels = s->preview_pixels ? s->preview_pixels : s->pixels;
            const auto base = fit_image(r, {float(pixels->width()), float(pixels->height())}, ImageFit::Contain);
            s->base_preview = base;
            const auto transform =
                clamp_image_transform(s->transform.get(), {base.width, base.height}, {r.width, r.height}, s->options);
            s->transform.set(transform);
            const auto w = base.width * transform.scale;
            const auto h = base.height * transform.scale;
            graphics::ImageInstance instance{
                pixels,
                {r.x + (r.width - w) / 2 + transform.x, r.y + (r.height - h) / 2 + transform.y, w, h},
                r,
                {r.x + r.width / 2 + transform.x, r.y + r.height / 2 + transform.y},
                transform.rotate,
                services_->nodes().content_opacity(n) * services_->nodes().paint_opacity(n),
                transform.flip_x,
                transform.flip_y};
            if (!s->preview_image) {
                s->preview_image = services_->images().create(instance);
            } else {
                services_->images().update(*s->preview_image, instance);
            }
            commands.push_back(
                {graphics::SceneDrawKind::image, s->preview_image->index, 1, s->preview_image->generation});
        }
        if (commands.empty() && s->preview_image) {
            services_->images().destroy(*s->preview_image);
            s->preview_image.reset();
        }
        services_->scene_composer().set_fragment(s->preview_fragment, commands, s->view.canvas_interaction, r);
    }
    if (!shown || s->disabled || !active_) {
        s->drag.reset();
        if (s->ticket && !s->ticket->completed) {
            cancel_load(id);
        }
    }
}

void ImageComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<ImageState>(id);
        if (s && s->ready && s->defer_load) {
            s->defer_load = false;
            if (s->needs_load) {
                const auto source = s->source;
                load(id, source);
            }
        }
    }
}

void ImageComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        publish(id, viewport, clip);
    }
}

void ImageComponentHost::select(runtime::ComponentId id, std::size_t index, bool request) {
    auto* s = services_->components().state<ImageState>(id);
    if (!s || s->items.empty()) {
        return;
    }
    index = std::min(index, s->items.size() - 1);
    const auto old = s->current.get();
    const auto callback = s->on_change;
    if (!request || !s->controlled_index) {
        const auto item = s->items[index].item;
        const bool unchanged = s->selected_key == item.key && s->source == item.source && !s->needs_load;
        s->current.set(index);
        s = services_->components().state<ImageState>(id);
        if (!s) {
            return;
        }
        s->desired_index = index;
        s->selected_key = item.key;
        s->source = item.source;
        s->view.alt = item.alt;
        s->alt.set(item.alt);
        s = services_->components().state<ImageState>(id);
        if (!s) {
            return;
        }
        if (!unchanged) {
            s->transform.set({});
            s = services_->components().state<ImageState>(id);
            if (!s) {
                return;
            }
            const auto source = s->source;
            load(id, source);
        }
    }
    if (request && callback && services_->components().contains(id)) {
        try {
            callback(index, old);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

void ImageComponentHost::reconcile(runtime::ComponentId id, const std::vector<ImagePreviewItem>& items) {
    validate_items(items);
    auto* s = services_->components().state<ImageState>(id);
    if (!s) {
        return;
    }
    const auto revision = ++s->group_revision;
    const auto original = s->items;
    const auto valid = [this, id, revision] {
        const auto* state = services_->components().state<ImageState>(id);
        return state && state->group_revision == revision;
    };
    std::vector<Thumbnail> next;
    std::vector<runtime::ComponentId> created;
    next.reserve(items.size());
    const auto old_key =
        s->items.empty() ? String{} : s->items[std::min(s->current.get(), s->items.size() - 1)].item.key;
    try {
        for (const auto& item : items) {
            const auto old = std::ranges::find_if(original, [&](const auto& v) { return v.item.key == item.key; });
            if (old != original.end()) {
                next.push_back(*old);
                next.back().item = item;
                continue;
            }
            Thumbnail thumb;
            thumb.item = item;
            thumb.source.set(item.source);
            thumb.alt.set(item.alt);
            append(*services_, s->mounting, id,
                   Content{[&, source = thumb.source, alt = thumb.alt, key = item.key, side = s->thumbnail_size] {
                       const auto before = mounted_.size();
                       auto thumbnail = ImageProps{}
                                            .source(source)
                                            .fallback(s->fallback_signal)
                                            .visible(s->thumbnails_visible)
                                            .alt(alt)
                                            .locale(s->labels)
                                            .preview(true)
                                            .onOpenChange([this, id, key](bool open) {
                                                if (!open) {
                                                    return;
                                                }
                                                auto* s = services_->components().state<ImageState>(id);
                                                if (!s) {
                                                    return;
                                                }
                                                auto it = std::ranges::find_if(
                                                    s->items, [&](const auto& v) { return v.item.key == key; });
                                                if (it != s->items.end()) {
                                                    select(id, it - s->items.begin(), true);
                                                    request_open(id, true);
                                                }
                                            })
                                            .open(false)
                                            .layout(LayoutStyle{}.width(side).height(side));
                       thumbnail.inherit_empty_alt_ = true;
                       Image(std::move(thumbnail));
                       if (mounted_.size() != before + 1) {
                           throw std::logic_error("Image thumbnail failed to mount");
                       }
                       thumb.component = mounted_.back();
                   }});
            created.push_back(thumb.component);
            next.push_back(std::move(thumb));
        }
    } catch (...) {
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        throw;
    }
    s = services_->components().state<ImageState>(id);
    if (!s || !valid()) {
        for (auto child : created) {
            services_->retire_failed_component(child);
        }
        return;
    }
    std::vector<runtime::ComponentId> removed;
    for (const auto& old : s->items) {
        if (std::ranges::none_of(next, [&](const auto& v) { return v.item.key == old.item.key; })) {
            removed.push_back(old.component);
        }
    }
    s->items = std::move(next);
    for (auto child : removed) {
        services_->destroy(child);
    }
    if (!valid()) {
        return;
    }
    s = services_->components().state<ImageState>(id);
    const auto rows = s->items;
    s->total.set(rows.size());
    if (!valid()) {
        return;
    }
    for (const auto& row : rows) {
        row.source.set(row.item.source);
        if (!valid()) {
            return;
        }
        row.alt.set(row.item.alt);
        if (!valid()) {
            return;
        }
    }
    s = services_->components().state<ImageState>(id);
    const auto prop = s->current_prop;
    const auto prior_index = s->desired_index;
    const auto desired = prop ? read_prop(*prop) : prior_index;
    if (!valid()) {
        return;
    }
    std::size_t current = std::min(desired, s->items.empty() ? std::size_t{} : s->items.size() - 1);
    if (!s->controlled_index && !old_key.empty()) {
        auto it = std::ranges::find_if(s->items, [&](const auto& v) { return v.item.key == old_key; });
        if (it != s->items.end()) {
            current = it - s->items.begin();
        }
    }
    s = services_->components().state<ImageState>(id);
    if (!s || !valid()) {
        return;
    }
    if (s->items.empty()) {
        cancel_load(id);
        s = services_->components().state<ImageState>(id);
        if (s && valid()) {
            s->pixels.reset();
            s->source = {};
            s->selected_key = {};
            if (s->preview_image) {
                services_->images().destroy(*s->preview_image);
                s->preview_image.reset();
            }
            s->view.status = ImageStatus::Empty;
            if (!s->controlled_open) {
                s->desired_open = false;
            }
            s->current.set(0);
            refresh(id);
        }
    } else {
        select(id, current, false);
    }
    refresh(id);
}

void ImageComponentHost::mount(const PreviewGroupProps& p) {
    validate_items(read_prop(p.items_));
    validate_image_preview(read_prop(p.preview_));
    auto& b = runtime::require_component_build_context();
    const auto locale = localized_prop(p.locale_, b.config_scope(), &LocaleData::image);
    const auto id = b.mount_component<ImageState>();
    auto& s = b.state<ImageState>(id);
    s.node = b.root(id);
    s.group = true;
    s.show_thumbnails = read_prop(p.thumbnails_);
    s.controlled_open = p.open_.has_value();
    s.controlled_index = p.current_.has_value();
    s.current_prop = p.current_;
    s.desired_open = p.open_ ? read_prop(*p.open_) : p.default_open_;
    s.desired_index = p.current_ ? read_prop(*p.current_) : p.default_current_;
    s.fallback = read_prop(p.fallback_);
    s.fallback_signal.set(s.fallback);
    s.options = read_prop(p.preview_);
    s.visible = read_prop(p.visible_);
    s.disabled = read_prop(p.disabled_);
    s.rtl = read_prop(p.rtl_);
    s.locale = read_prop(locale);
    s.renderer = read_prop(p.actions_);
    s.on_group_open = p.on_open_;
    s.on_change = p.on_change_;
    s.on_transform = p.on_transform_;
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services_->nodes(), services_->dirty());
    b.on_resource_cleanup(id, [this, id, node = s.node] {
        cancel_load(id);
        cancel_preview(id);
        if (const auto* state = services_->components().state<ImageState>(id); state && state->preview_image) {
            services_->images().destroy(*state->preview_image);
        }
        services_->layout().remove_layout(node);
    });
    mount_preview(id);
    if (!services_->components().contains(id)) {
        return;
    }
    services_->layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) { return measure(id, e, c); },
            [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) { place(id, e, r); }});
    mounted_.push_back(id);
    s.ready = true;
    auto& scope = b.scope(id);
    connect_prop(scope, p.fallback_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->fallback = v;
            s->fallback_signal.set(std::move(v));
        }
    });
    connect_prop(scope, p.thumbnails_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->show_thumbnails = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.items_, [this, id](const std::vector<ImagePreviewItem>& v) { reconcile(id, v); });
    if (p.current_) {
        connect_prop(scope, *p.current_, [this, id, items = p.items_](std::size_t v) {
            if (auto* s = services_->components().state<ImageState>(id)) {
                s->desired_index = v;
                const auto latest = read_prop(items);
                reconcile(id, latest);
            }
        });
    }
    if (p.open_) {
        connect_prop(scope, *p.open_, [this, id](bool v) {
            if (auto* s = services_->components().state<ImageState>(id)) {
                s->desired_open = v;
                refresh(id);
            }
        });
    }
    connect_prop(scope, p.preview_, [this, id](ImagePreviewOptions v) {
        validate_image_preview(v);
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->options = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->visible = v;
            if (!v) {
                cancel_load(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->disabled = v;
            if (v) {
                cancel_load(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.rtl_, [this, id](bool v) {
        if (auto* s = services_->components().state<ImageState>(id)) {
            s->rtl = v;
            refresh(id);
        }
    });
    connect_prop(scope, locale, [this, id](ImageLocale v) { update_locale(id, std::move(v)); });
    connect_prop(scope, p.actions_,
                 [this, id](std::shared_ptr<const ImageActionsRenderer> v) { replace_actions(id, std::move(v)); });
    const auto theme = b.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::image_colors, &theme_runtime::ThemeScope::image_metrics,
                               &theme_runtime::ThemeScope::image_typography, &theme_runtime::ThemeScope::image_effects,
                               &theme_runtime::ThemeScope::image_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, i](theme_runtime::DirtyPhase) { refresh(id, i != 0); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    s.mounting = false;
    refresh(id);
}
} // namespace ryn::detail

namespace ryn {
ImageRef::ImageRef() : state_(std::make_shared<detail::ImageRefState>()) {}

bool ImageRef::bound() const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->bound;
    return f && f();
}

bool ImageRef::focus() const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->focus;
    return f && f();
}

bool ImageRef::preview(bool v) const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->open;
    return f && f(v);
}

bool ImageRef::action(ImageAction v) const {
    if (state_->owner != std::this_thread::get_id()) {
        return false;
    }
    const auto f = state_->action;
    return f && f(v);
}

void Image(ImageProps p, ImageSlots slots) {
    detail::ConfigDefaults::apply(p);
    if (!detail::active_image) {
        throw std::logic_error("Image requires WindowComponentServices");
    }
    detail::active_image->mount(p, slots);
}

void PreviewGroup(PreviewGroupProps p) {
    detail::ConfigDefaults::apply(p);
    if (!detail::active_image) {
        throw std::logic_error("PreviewGroup requires WindowComponentServices");
    }
    detail::active_image->mount(p);
}
} // namespace ryn
