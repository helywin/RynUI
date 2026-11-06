#include "component/qrcode_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/localized_prop.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/spin.hpp>
#include <ryn/text.hpp>
#include <algorithm>
#include <cmath>
#include <thread>

namespace ryn::detail {
struct QRCodeRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t binding{};
    std::function<bool()> bound;
    std::function<bool()> refresh;
    std::function<std::optional<QRCodeData>()> data;
    std::function<QRCodePaint()> paint;

    void check() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("QRCodeRef requires owner thread");
        }
    }
};

struct QRCodeComponentHost::Lifetime final {
    QRCodeComponentHost* host{};
    std::thread::id owner{std::this_thread::get_id()};
};

namespace {
thread_local QRCodeComponentHost* active_qrcode{};

struct LogoTicket final {
    std::thread::id owner{std::this_thread::get_id()};
    bool active{true};
    bool completed{};
};

struct QRCodeState final {
    QRCodeProps props;
    runtime::NodeId node;
    QRCodeSnapshot view;
    QRCodeValue value;
    QRCodeEncoding encoding;
    unsigned margin{4};
    float size{160};
    QRCodeIconSize icon_size;
    QRCodeLocale locale;
    ImageSource source;
    std::shared_ptr<const ImagePixels> logo;
    std::shared_ptr<LogoTicket> ticket;
    ImageCancel cancel;
    std::uint64_t logo_generation{};
    std::uint64_t revision{};
    std::uint64_t status_generation{};
    bool needs_logo{true};
    bool visible{true};
    bool disabled{};
    bool bordered{true};
    bool ready{};
    bool dirty{true};
    bool publishing{};
    bool building_status{};
    bool needs_status{true};
    std::optional<Color> foreground;
    std::optional<Color> background;
    std::shared_ptr<const QRCodeStatusRenderer> renderer;
    Signal<QRCodeStatus> status{QRCodeStatus::Active};
    Signal<QRCodeLocale> labels{QRCodeLocale{}};
    Signal<bool> button_disabled{false};
    Signal<bool> spinning{false};
    Signal<String> message{String{}};
    Signal<runtime::SemanticForeground> text{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    component::RetainedSurfaceId background_surface;
    component::RetainedSurfaceId overlay_surface;
    std::array<component::RetainedSurfaceId, 6> module_surfaces;
    runtime::SceneFragmentId logo_fragment;
    std::optional<graphics::ImageId> logo_image;
    runtime::ComponentId status_root;
    runtime::ComponentId spinner;
    runtime::ComponentId message_root;
    std::array<theme_runtime::Subscription, 2> subscriptions;
    runtime::Rect last_bounds;
    runtime::Rect last_clip;
};

graphics::QuadInstance quad(runtime::Rect r, Color c, float radius = 0) {
    graphics::QuadInstance q;
    q.bounds = {r.x, r.y, r.width, r.height};
    q.color = {c.red(), c.green(), c.blue(), c.alpha()};
    q.corner_radius = radius;
    return q;
}

runtime::Rect intersection(runtime::Rect a, runtime::Rect b) {
    const auto x = std::max(a.x, b.x);
    const auto y = std::max(a.y, b.y);
    return {x, y, std::max(0.F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.F, std::min(a.y + a.height, b.y + b.height) - y)};
}

runtime::ComponentId region(WindowComponentServices& s) {
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<int>(0);
    const auto node = b.root(id);
    s.layout().set_layout(node, layout::ComponentLayout{
                                    [&s](layout::LayoutEngine& e, runtime::NodeId n, layout::Constraints c) {
                                        runtime::Size size;
                                        for (auto child : s.nodes().require(n).children) {
                                            auto v = e.measure_child(child, c);
                                            size.width = std::max(size.width, v.width);
                                            size.height = std::max(size.height, v.height);
                                        }
                                        return c.constrain(size);
                                    },
                                    [&s](layout::LayoutEngine& e, runtime::NodeId n, runtime::Rect r) {
                                        for (auto child : s.nodes().require(n).children) {
                                            const auto z = s.nodes().require(child).layout_size;
                                            e.place_child(child, {r.x + (r.width - z.width) / 2,
                                                                  r.y + (r.height - z.height) / 2, z.width, z.height});
                                        }
                                    }});
    b.on_resource_cleanup(id, [&s, id, node] {
        s.interactions().set_input_blocked(id, false);
        s.layout().remove_layout(node);
    });
    return id;
}

void append(WindowComponentServices& s, runtime::ComponentId parent, const Content& content) {
    runtime::ComponentBuildContext* b{};
    try {
        b = &runtime::require_component_build_context();
    } catch (const std::logic_error&) {
    }
    if (b && &b->host() == &s.components()) {
        b->mount_slot_with_theme_scope(
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

void cancel(ImageCancel fn) noexcept {
    if (fn) {
        try {
            fn();
        } catch (...) {
        }
    }
}
} // namespace

QRCodeComponentHost::QRCodeComponentHost(WindowComponentServices& s)
    : services_(&s), lifetime_(std::make_shared<Lifetime>()) {
    lifetime_->host = this;
    s.attach(*this);
}

QRCodeComponentHost::~QRCodeComponentHost() {
    lifetime_->host = nullptr;
    services_->detach(*this);
}

void* QRCodeComponentHost::begin_mount() noexcept {
    return std::exchange(active_qrcode, this);
}

void QRCodeComponentHost::end_mount(void* p) noexcept {
    active_qrcode = static_cast<QRCodeComponentHost*>(p);
}

void QRCodeComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void QRCodeComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

QRCodeSnapshot QRCodeComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        throw std::out_of_range("Stale QRCode");
    }
    auto result = s->view;
    result.paint = paint(id);
    result.logo_image = s->logo_image;
    return result;
}

void QRCodeComponentHost::fail(runtime::ComponentId id, QRCodeError error) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const bool changed = s->view.error != error;
    s->view.error = error;
    s->dirty = true;
    if (changed && s->props.on_error_) {
        auto cb = s->props.on_error_;
        try {
            cb(error);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

void QRCodeComponentHost::encode(runtime::ComponentId id) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const auto value = s->value;
    const auto options = s->encoding;
    ++s->view.encoding_revision;
    try {
        auto data = QRCodeData::encode(value, options);
        s->view.data = std::move(data);
        s->view.error.reset();
        s->dirty = true;
    } catch (const std::exception& e) {
        s->view.data.reset();
        fail(id, {QRCodeErrorCode::Encoding, e.what()});
    }
}

QRCodePaint QRCodeComponentHost::paint(runtime::ComponentId id) const {
    const auto* s = services_->components().state<QRCodeState>(id);
    if (!s || !services_->components().scope(id).active()) {
        throw std::logic_error("QRCode is retired");
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().qrcode();
    return {s->foreground.value_or(t.colors.foreground),
            s->background.value_or(t.colors.background),
            s->margin,
            s->logo,
            std::min(s->icon_size.width, s->size),
            std::min(s->icon_size.height, s->size),
            s->size,
            true,
            1};
}

bool QRCodeComponentHost::request_refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s || !active_ || !s->visible || s->disabled || !services_->interactions().component_active(id) ||
        s->view.status != QRCodeStatus::Expired || !s->props.on_refresh_) {
        return false;
    }
    const auto cb = s->props.on_refresh_;
    try {
        cb();
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    return services_->components().contains(id);
}

void QRCodeComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    ++s->revision;
    s->dirty = true;
    if (geometry) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    services_->request_frame();
}

void QRCodeComponentHost::cancel_logo(runtime::ComponentId id) noexcept {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    ++s->logo_generation;
    if (s->ticket) {
        s->ticket->active = false;
        if (!s->ticket->completed) {
            s->needs_logo = true;
        }
    }
    s->ticket.reset();
    auto fn = std::move(s->cancel);
    cancel(std::move(fn));
}

void QRCodeComponentHost::load_logo(runtime::ComponentId id) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s || !s->ready || !s->needs_logo || !active_ || !s->visible || s->disabled ||
        !services_->interactions().component_active(id) || s->view.bounds.width <= 0) {
        return;
    }
    const auto source = s->source;
    const auto expected = s->logo_generation + 1;
    cancel_logo(id);
    s = services_->components().state<QRCodeState>(id);
    if (!s || s->logo_generation != expected) {
        return;
    }
    s->needs_logo = false;
    s->logo.reset();
    s->dirty = true;
    if (source.value.index() == 0) {
        s->view.logo_status = ImageStatus::Empty;
        return;
    }
    s->view.logo_status = ImageStatus::Loading;
    auto ticket = std::make_shared<LogoTicket>();
    s->ticket = ticket;
    const std::weak_ptr<Lifetime> weak = lifetime_;
    ImageCompletion done = [weak, id, ticket, expected](ImageDecodeResult result) {
        if (ticket->owner != std::this_thread::get_id() || !ticket->active || ticket->completed) {
            return false;
        }
        const auto life = weak.lock();
        if (!life || !life->host) {
            return false;
        }
        auto& h = *life->host;
        auto* s = h.services_->components().state<QRCodeState>(id);
        if (!s || s->logo_generation != expected) {
            return false;
        }
        ticket->completed = true;
        s->cancel = {};
        s->logo = result.pixels;
        s->view.logo_status = result.pixels ? ImageStatus::Ready : ImageStatus::Error;
        if (result.pixels && s->view.error && s->view.error->code == QRCodeErrorCode::Logo) {
            s->view.error.reset();
        }
        h.refresh(id, false);
        if (!result.pixels) {
            h.fail(id, {QRCodeErrorCode::Logo, result.error.message});
        } else if (s->props.on_icon_load_) {
            const auto cb = s->props.on_icon_load_;
            try {
                cb();
            } catch (...) {
                h.services_->retire_failed_component(id);
                throw;
            }
        }
        return true;
    };
    try {
        if (const auto* v = std::get_if<std::shared_ptr<const ImagePixels>>(&source.value)) {
            done({*v, *v ? ImageError{} : ImageError{ImageErrorCode::empty, "Null logo pixels"}});
        } else if (const auto* v = std::get_if<std::shared_ptr<const EncodedImage>>(&source.value)) {
            done(*v ? decode_image(**v) : ImageDecodeResult{{}, {ImageErrorCode::empty, "Null logo bytes"}});
        } else {
            const auto loader = std::get<std::shared_ptr<const ImageLoader>>(source.value);
            if (!loader || !loader->start) {
                done({{}, {ImageErrorCode::loader_failure, "No logo loader"}});
                return;
            }
            auto fn = loader->start(done);
            s = services_->components().state<QRCodeState>(id);
            if (s && s->logo_generation == expected && ticket->active && !ticket->completed) {
                s->cancel = std::move(fn);
            } else if (!ticket->completed) {
                cancel(std::move(fn));
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
            done({{}, {ImageErrorCode::loader_failure, "Logo loader threw"}});
        } else {
            throw;
        }
    }
}

void QRCodeComponentHost::status_content(runtime::ComponentId id) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s || !s->needs_status || s->building_status) {
        return;
    }
    s->needs_status = false;
    s->building_status = true;
    const auto revision = s->revision;
    const auto factory_generation = ++s->status_generation;
    const auto renderer = s->renderer;
    const auto previous = s->status_root;
    s->status_root = {};
    s->view.status_content = {};
    s->view.refresh = {};
    s->spinner = {};
    s->message_root = {};
    if (previous.valid()) {
        services_->destroy(previous);
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s || s->status_generation != factory_generation) {
        return;
    }
    runtime::ComponentId replacement;
    try {
        append(*services_, id, Content{[&] {
            auto& b = runtime::require_component_build_context();
            replacement = region(*services_);
            b.mount_slot_with_semantic_text_style(
                replacement, Content{[&] {
                    FormPrivateEditorScope private_controls;
                    if (renderer && renderer->render) {
                        const auto content =
                            renderer->render({s->status, s->labels, s->button_disabled, [this, id, factory_generation] {
                                                  const auto* current = services_->components().state<QRCodeState>(id);
                                                  return current && current->status_generation == factory_generation &&
                                                         request_refresh(id);
                                              }});
                        if (services_->components().contains(id)) {
                            b.mount_slot(replacement, content);
                        }
                        return;
                    }
                    Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                             s->spinner = region(*services_);
                             b.mount_slot(s->spinner, Content{[&] { Spin(SpinProps{}.spinning(s->spinning)); }});
                             s->message_root = region(*services_);
                             b.mount_slot(s->message_root, Content{[&] { Text(TextProps{}.content(s->message)); }});
                             s->view.refresh = region(*services_);
                             b.mount_slot(s->view.refresh, Content{[&] {
                                              Button(ButtonProps{}
                                                         .type(ButtonType::Link)
                                                         .disabled(s->button_disabled)
                                                         .onClick([this, id] { request_refresh(id); }),
                                                     ButtonContent{[labels = s->labels] {
                                                         Text(TextProps{}.content(
                                                             bind([labels] { return labels.get().refresh; })));
                                                     }});
                                          }});
                         }});
                }},
                s->text, s->font);
        }});
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const auto old = s->status_root;
    s->status_root = replacement;
    s->view.status_content = replacement;
    s->building_status = false;
    if (old.valid()) {
        services_->destroy(old);
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    if (s->revision != revision && s->renderer != renderer) {
        s->needs_status = true;
    }
    refresh(id);
}

void QRCodeComponentHost::mount(const QRCodeProps& p) {
    auto& b = runtime::require_component_build_context();
    const auto locale = localized_prop(p.locale_, b.config_scope(), &LocaleData::qrcode);
    const auto id = b.mount_component<QRCodeState>();
    auto& s = b.state<QRCodeState>(id);
    s.node = b.root(id);
    s.props = p;
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, services_->nodes(), services_->dirty());
    auto fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.background_surface = services_->surfaces().create_content_range(fragment, {});
    for (auto& range : s.module_surfaces) {
        fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
        range = services_->surfaces().create_content_range(fragment, {});
    }
    s.logo_fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.overlay_surface = services_->surfaces().create_content_range(fragment, {});
    b.on_resource_cleanup(id, [this, id, node = s.node, bg = s.background_surface, overlay = s.overlay_surface,
                               ranges = s.module_surfaces] {
        cancel_logo(id);
        if (auto* s = services_->components().state<QRCodeState>(id); s && s->logo_image) {
            services_->images().destroy(*s->logo_image);
        }
        services_->interactions().set_input_blocked(id, false);
        services_->layout().remove_layout(node);
        services_->surfaces().destroy_content_range(bg);
        services_->surfaces().destroy_content_range(overlay);
        for (auto range : ranges) {
            services_->surfaces().destroy_content_range(range);
        }
    });
    services_->layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& e, runtime::NodeId, layout::Constraints c) {
                                            auto* s = services_->components().state<QRCodeState>(id);
                                            const auto size = c.constrain({s->size, s->size});
                                            if (s->status_root.valid()) {
                                                e.measure_child(services_->components().root(s->status_root),
                                                                {0, size.width, 0, size.height});
                                            }
                                            return size;
                                        },
                                        [this, id](layout::LayoutEngine& e, runtime::NodeId, runtime::Rect r) {
                                            auto* s = services_->components().state<QRCodeState>(id);
                                            if (s->status_root.valid()) {
                                                const auto child = services_->components().root(s->status_root);
                                                const auto z = services_->nodes().require(child).layout_size;
                                                e.place_child(child,
                                                              {r.x + (r.width - z.width) / 2,
                                                               r.y + (r.height - z.height) / 2, z.width, z.height});
                                            }
                                        }});
    auto& scope = b.scope(id);
    const auto update = [this, id] {
        if (auto* s = services_->components().state<QRCodeState>(id); s && s->ready) {
            encode(id);
            refresh(id);
        }
    };
    connect_prop(scope, p.value_, [this, id, update](QRCodeValue v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->value = std::move(v);
            update();
        }
    });
    connect_prop(scope, p.errorLevel_, [this, id, update](QRCodeErrorLevel v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->encoding.error_level = v;
            update();
        }
    });
    connect_prop(scope, p.boostLevel_, [this, id, update](bool v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->encoding.boost_level = v;
            update();
        }
    });
    connect_prop(scope, p.minVersion_, [this, id, update](int v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->encoding.min_version = v;
            update();
        }
    });
    connect_prop(scope, p.marginSize_, [this, id](unsigned v) {
        if (v > 64) {
            throw std::invalid_argument("QRCode margin exceeds 64 modules");
        }
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->margin = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.size_, [this, id](float v) {
        if (!std::isfinite(v) || v < 0 || v > 16384) {
            throw std::invalid_argument("QRCode size must be finite in [0,16384]");
        }
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->size = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.iconSize_, [this, id](QRCodeIconSize v) {
        if (!std::isfinite(v.width) || !std::isfinite(v.height) || v.width < 0 || v.height < 0 || v.width > 16384 ||
            v.height > 16384) {
            throw std::invalid_argument("QRCode icon size");
        }
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->icon_size = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.status_, [this, id](QRCodeStatus v) {
        if (v < QRCodeStatus::Active || v > QRCodeStatus::Scanned) {
            throw std::invalid_argument("QRCode status");
        }
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->view.status = v;
            s->status.set(v);
            refresh(id);
        }
    });
    connect_prop(scope, locale, [this, id](QRCodeLocale v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->locale = v;
            const auto labels = s->labels;
            refresh(id);
            // The public status renderer can retire the owner or switch locale.
            // Do not touch the borrowed State after publishing its Signal.
            labels.set(std::move(v));
        }
    });
    connect_prop(scope, p.bordered_, [this, id](bool v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->bordered = v;
            refresh(id);
        }
    });
    connect_prop(scope, p.visible_, [this, id](bool v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->visible = v;
            if (!v) {
                cancel_logo(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.disabled_, [this, id](bool v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->disabled = v;
            if (v) {
                cancel_logo(id);
            }
            refresh(id);
        }
    });
    connect_prop(scope, p.icon_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<QRCodeState>(id); s && s->source != v) {
            s->source = std::move(v);
            s->needs_logo = true;
            cancel_logo(id);
            refresh(id);
        }
    });
    if (p.color_) {
        connect_prop(scope, *p.color_, [this, id](Color v) {
            if (auto* s = services_->components().state<QRCodeState>(id)) {
                s->foreground = v;
                refresh(id, false);
            }
        });
    }
    if (p.background_) {
        connect_prop(scope, *p.background_, [this, id](Color v) {
            if (auto* s = services_->components().state<QRCodeState>(id)) {
                s->background = v;
                refresh(id, false);
            }
        });
    }
    connect_prop(scope, p.renderer_, [this, id](std::shared_ptr<const QRCodeStatusRenderer> v) {
        if (auto* s = services_->components().state<QRCodeState>(id)) {
            s->renderer = std::move(v);
            s->needs_status = true;
            refresh(id);
        }
    });
    const auto theme = b.theme_scope();
    s.subscriptions[0] =
        theme->capture([this, id](auto) { refresh(id, false); }, [&] { (void)theme->qrcode_colors(); });
    s.subscriptions[1] = theme->capture([this, id](auto) { refresh(id); }, [&] { (void)theme->qrcode_metrics(); });
    s.ready = true;
    mounted_.push_back(id);
    encode(id);
    if (!services_->components().contains(id)) {
        return;
    }
    status_content(id);
    if (p.ref_) {
        const auto ref = p.ref_->state_;
        ref->check();
        if (ref->bound && ref->bound()) {
            throw std::invalid_argument("QRCodeRef already bound");
        }
        const auto binding = ++ref->binding;
        const std::weak_ptr<Lifetime> weak = lifetime_;
        const auto host = [weak]() -> QRCodeComponentHost* {
            const auto life = weak.lock();
            return life && life->owner == std::this_thread::get_id() ? life->host : nullptr;
        };
        ref->bound = [host, id] {
            auto* h = host();
            return h && h->services_->components().contains(id) && h->services_->components().scope(id).active();
        };
        ref->refresh = [host, id] {
            auto* h = host();
            return h && h->request_refresh(id);
        };
        ref->data = [host, id]() -> std::optional<QRCodeData> {
            auto* h = host();
            if (!h) {
                return {};
            }
            auto* s = h->services_->components().state<QRCodeState>(id);
            return s && h->services_->components().scope(id).active() ? s->view.data : std::nullopt;
        };
        ref->paint = [host, id] {
            auto* h = host();
            if (!h) {
                throw std::logic_error("QRCode is retired");
            }
            return h->paint(id);
        };
        b.on_resource_cleanup(id, [ref, binding] {
            if (ref->binding == binding) {
                ref->bound = {};
                ref->refresh = {};
                ref->data = {};
                ref->paint = {};
            }
        });
    }
}

void QRCodeComponentHost::publish(runtime::ComponentId id, runtime::Size viewport, runtime::Rect document_clip) {
    auto* s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const auto& n = services_->nodes().require(s->node);
    const runtime::Rect bounds{n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width,
                               n.bounds.height};
    const auto clip = services_->component_clip(id, viewport, document_clip);
    const bool shown = s->visible && services_->components().branch_active(id) && bounds.width > 0 &&
                       bounds.height > 0 && intersection(bounds, clip).width > 0 &&
                       intersection(bounds, clip).height > 0;
    if (!shown || !active_ || !services_->interactions().component_active(id)) {
        cancel_logo(id);
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    s->view.bounds = bounds;
    if (shown) {
        load_logo(id);
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const bool covered = shown && s->view.status != QRCodeStatus::Active;
    if (s->status_root.valid()) {
        services_->components().set_branch_active(s->status_root, covered);
        services_->set_input_blocked(s->status_root, !covered || s->disabled || !active_);
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s) {
        return;
    }
    const auto revision = s->revision;
    const auto spinning = s->spinning;
    const auto disabled = s->button_disabled;
    const auto message = s->message;
    const bool spinning_value = covered && active_ && !s->disabled && s->view.status == QRCodeStatus::Loading;
    const bool disabled_value = s->disabled || !active_;
    const auto message_value = s->view.status == QRCodeStatus::Expired   ? s->locale.expired
                               : s->view.status == QRCodeStatus::Scanned ? s->locale.scanned
                                                                         : s->locale.loading;
    const auto emit = [this, id, revision](auto signal, auto value) {
        signal.set(std::move(value));
        const auto* current = services_->components().state<QRCodeState>(id);
        return current && current->revision == revision;
    };
    if (!emit(spinning, spinning_value) || !emit(disabled, disabled_value) || !emit(message, message_value)) {
        return;
    }
    s = services_->components().state<QRCodeState>(id);
    if (!s->renderer) {
        if (s->spinner.valid()) {
            services_->components().set_branch_active(s->spinner, covered && s->view.status == QRCodeStatus::Loading);
        }
        if (s->message_root.valid()) {
            services_->components().set_branch_active(s->message_root,
                                                      covered && s->view.status != QRCodeStatus::Loading);
        }
        if (s->view.refresh.valid()) {
            services_->components().set_branch_active(
                s->view.refresh, covered && s->view.status == QRCodeStatus::Expired && bool(s->props.on_refresh_));
        }
    }
    if (!s->dirty && s->last_bounds == bounds && s->last_clip == clip) {
        return;
    }
    s->dirty = false;
    s->last_bounds = bounds;
    s->last_clip = clip;
    const auto& t = services_->components().theme_scope(id)->snapshot().qrcode();
    const auto text = s->text;
    const auto font = s->font;
    if (!emit(text, runtime::SemanticForeground{t.colors.text.red(), t.colors.text.green(), t.colors.text.blue(),
                                                t.colors.text.alpha()}) ||
        !emit(font, runtime::SemanticTypography{t.metrics.font_family, 400, false, t.metrics.font_size,
                                                t.metrics.line_height})) {
        return;
    }
    s = services_->components().state<QRCodeState>(id);
    const float length = std::max(0.F, std::min(bounds.width, bounds.height));
    const runtime::Rect square{bounds.x + (bounds.width - length) / 2, bounds.y + (bounds.height - length) / 2, length,
                               length};
    const float padding = s->bordered ? std::min(length / 2, t.metrics.padding + t.metrics.border_width) : 0;
    const runtime::Rect inner{square.x + padding, square.y + padding, std::max(0.F, length - 2 * padding),
                              std::max(0.F, length - 2 * padding)};
    auto p = paint(id);
    p.logo_width = std::min(p.logo_width, p.size);
    p.logo_height = std::min(p.logo_height, p.size);
    const float opacity = services_->nodes().content_opacity(s->node);
    const bool valid_paint = p.foreground != p.background && p.foreground.alpha() > 0;
    if (!valid_paint && s->view.data) {
        const auto revision = s->revision;
        fail(id, {QRCodeErrorCode::Paint, "QRCode requires distinct colors and visible foreground"});
        s = services_->components().state<QRCodeState>(id);
        if (!s || s->revision != revision) {
            return;
        }
        s->dirty = false;
    } else if (s->view.error && s->view.error->code == QRCodeErrorCode::Paint) {
        s->view.error.reset();
    }
    std::vector<graphics::QuadInstance> bg;
    if (shown) {
        bg.push_back(quad(square, p.background, s->bordered ? t.metrics.radius : 0));
        bg.back().opacity = opacity;
    }
    static_cast<void>(services_->surfaces().update_content_range(s->background_surface, bg));
    std::vector<graphics::RoundedEffectInstance> border;
    if (shown && s->bordered && t.metrics.border_width > 0) {
        const auto effects = graphics::make_corner_outline_effects({square, t.metrics.radius}, {true, true, true, true},
                                                                   t.metrics.border_width, 0, t.colors.border, opacity,
                                                                   {}, graphics::EffectClip{1, clip});
        border.assign(effects.begin(), effects.end());
    }
    services_->surfaces().update_content_effects(s->background_surface, border);
    s->view.modules.clear();
    s->view.symbol = {};
    s->view.logo = {};
    if (shown && s->view.data && inner.width > 0 && valid_paint) {
        const auto& d = *s->view.data;
        const float count = d.size() + 2.F * s->margin;
        const float pitch = inner.width / count;
        const float x0 = inner.x + s->margin * pitch;
        const float y0 = inner.y + s->margin * pitch;
        s->view.symbol = {x0, y0, pitch * d.size(), pitch * d.size()};
        const float lw = s->size > 0 ? std::min(s->view.symbol.width, p.logo_width / s->size * inner.width) : 0;
        const float lh = s->size > 0 ? std::min(s->view.symbol.height, p.logo_height / s->size * inner.height) : 0;
        s->view.logo = {x0 + (s->view.symbol.width - lw) / 2, y0 + (s->view.symbol.height - lh) / 2, lw, lh};
        const auto add = [&](runtime::Rect r) {
            r = intersection(r, clip);
            if (r.width > 0 && r.height > 0) {
                s->view.modules.push_back(quad(r, p.foreground));
                s->view.modules.back().opacity = opacity;
            }
        };
        for (int y = 0; y < d.size(); ++y) {
            for (int x = 0; x < d.size();) {
                if (!d.module(x, y)) {
                    ++x;
                    continue;
                }
                const int begin = x;
                while (d.module(x, y)) {
                    ++x;
                }
                const runtime::Rect r{x0 + begin * pitch, y0 + y * pitch, (x - begin) * pitch, pitch};
                const auto hole = s->logo ? intersection(r, s->view.logo) : runtime::Rect{};
                if (hole.width <= 0 || hole.height <= 0) {
                    add(r);
                } else {
                    add({r.x, r.y, r.width, hole.y - r.y});
                    add({r.x, hole.y + hole.height, r.width, r.y + r.height - hole.y - hole.height});
                    add({r.x, hole.y, hole.x - r.x, hole.height});
                    add({hole.x + hole.width, hole.y, r.x + r.width - hole.x - hole.width, hole.height});
                }
            }
        }
    }
    std::size_t offset = 0;
    for (auto range : s->module_surfaces) {
        const auto n = std::min<std::size_t>(4096, s->view.modules.size() - offset);
        static_cast<void>(
            services_->surfaces().update_content_range(range, std::span(s->view.modules).subspan(offset, n)));
        offset += n;
    }
    if (offset != s->view.modules.size()) {
        throw std::length_error("QRCode module surface capacity exceeded");
    }
    std::vector<graphics::SceneDrawCommand> commands;
    if (shown && s->logo && s->view.logo.width > 0 && s->view.logo.height > 0) {
        const auto r = s->view.logo;
        graphics::ImageInstance image{
            s->logo, r, intersection(clip, s->view.symbol), {r.x + r.width / 2, r.y + r.height / 2}};
        image.opacity = services_->nodes().content_opacity(s->node) * services_->nodes().paint_opacity(s->node);
        if (!s->logo_image) {
            s->logo_image = services_->images().create(image);
        } else {
            services_->images().update(*s->logo_image, image);
        }
        commands.push_back({graphics::SceneDrawKind::image, s->logo_image->index, 1, s->logo_image->generation});
    } else if (s->logo_image) {
        services_->images().destroy(*s->logo_image);
        s->logo_image.reset();
    }
    services_->scene_composer().set_fragment(s->logo_fragment, commands);
    std::vector<graphics::QuadInstance> overlay;
    if (covered) {
        overlay.push_back(quad(intersection(square, clip), t.colors.cover, s->bordered ? t.metrics.radius : 0));
        overlay.back().opacity = opacity;
    }
    static_cast<void>(services_->surfaces().update_content_range(s->overlay_surface, overlay));
}

void QRCodeComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        status_content(id);
    }
}

void QRCodeComponentHost::synchronize_auxiliary_geometry(runtime::Size v, runtime::Rect c) {
    const auto ids = mounted_;
    for (auto id : ids) {
        try {
            publish(id, v, c);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

void QRCodeComponentHost::on_window_active(bool v) {
    active_ = v;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!v) {
            cancel_logo(id);
        }
        refresh(id, false);
    }
}

void QRCodeComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->interactions().component_active(id)) {
            cancel_logo(id);
        }
        refresh(id, false);
    }
}
} // namespace ryn::detail

namespace ryn {
QRCodeRef::QRCodeRef() : state_(std::make_shared<detail::QRCodeRefState>()) {}

bool QRCodeRef::bound() const {
    state_->check();
    return state_->bound && state_->bound();
}

bool QRCodeRef::refresh() const {
    state_->check();
    return state_->refresh && state_->refresh();
}

std::optional<QRCodeData> QRCodeRef::data() const {
    state_->check();
    return state_->data ? state_->data() : std::nullopt;
}

std::vector<std::byte> QRCodeRef::exportPng(std::uint32_t size) const {
    const auto d = data();
    if (!d || !state_->paint) {
        throw std::logic_error("QRCode export has no live encoded data");
    }
    return export_qrcode_png(*d, state_->paint(), size);
}

std::string QRCodeRef::exportSvg() const {
    const auto d = data();
    if (!d || !state_->paint) {
        throw std::logic_error("QRCode export has no live encoded data");
    }
    return export_qrcode_svg(*d, state_->paint());
}

void QRCode(QRCodeProps p) {
    if (!detail::active_qrcode) {
        throw std::logic_error("QRCode requires WindowComponentServices");
    }
    detail::active_qrcode->mount(p);
}
} // namespace ryn
