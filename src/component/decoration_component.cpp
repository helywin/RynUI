#include "component/decoration_component.hpp"
#include "component/image_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <thread>

namespace ryn::detail {
struct DecorationComponentHost::Lifetime final {
    DecorationComponentHost* host{};
};

namespace {
thread_local DecorationComponentHost* active_decoration{};
constexpr auto dirty_geometry =
    runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest;

struct Ticket final {
    std::thread::id owner{std::this_thread::get_id()};
    bool active{true};
    bool completed{};
};

struct DecorationState final {
    DecorationKind kind{};
    runtime::NodeId node;
    runtime::ComponentId layer;
    runtime::NodeId layer_node;
    runtime::ComponentId content;
    runtime::NodeId content_node;
    runtime::SceneFragmentId fragment;
    component::RetainedSurfaceId effect;
    theme_runtime::Subscription subscription;
    bool visible{true};
    bool disabled{};
    bool ready{};
    AffixTarget target;
    std::optional<float> top;
    std::optional<float> bottom;
    bool affixed{};
    bool notify{};
    runtime::Rect visual;
    std::function<void(bool)> change;
    Duration duration{Duration::seconds(6)};
    BorderBeamDirection direction{BorderBeamDirection::Clockwise};
    double phase{};
    bool running{};
    bool has_run{};
    bool resuming{};
    bool onscreen{};
    animation::AnimationTime last;
    String text;
    WatermarkSize size;
    WatermarkGap gap;
    std::optional<WatermarkOffset> offset;
    float rotate{-22};
    ImageSource source;
    ImageStatus status{ImageStatus::Empty};
    ImageError error;
    bool needs_load{};
    bool report{};
    std::shared_ptr<Ticket> ticket;
    ImageCancel cancel;
    std::shared_ptr<const ImagePixels> pixels;
    std::function<void()> load;
    std::function<void(const ImageError&)> failure;
    std::optional<TextSceneId> master;
    std::vector<TextSceneId> texts;
    std::vector<graphics::ImageId> images;
};

void require_finite(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Decoration requires finite logical coordinates");
    }
}

void nonnegative(float value) {
    require_finite(value);
    if (value < 0) {
        throw std::invalid_argument("Decoration requires nonnegative dimensions");
    }
}

runtime::Rect world(const runtime::Node& n) {
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

bool enabled(WindowComponentServices& services, runtime::ComponentId id, const DecorationState& s) {
    return s.visible && !s.disabled && services.components().branch_active(id) &&
           services.interactions().component_active(id);
}

void invoke_cancel(ImageCancel fn) noexcept {
    if (fn) {
        try {
            fn();
        } catch (...) {
        }
    }
}

void clear_tiles(WindowComponentServices& services, DecorationState& s) {
    auto& scene = services.text().scene_service();
    for (auto id : s.texts) {
        scene.destroy(id);
    }
    s.texts.clear();
    for (auto id : s.images) {
        services.images().destroy(id);
    }
    s.images.clear();
}
} // namespace

DecorationComponentHost::DecorationComponentHost(WindowComponentServices& s)
    : services_(&s), lifetime_(std::make_shared<Lifetime>(Lifetime{this})) {
    s.attach(*this);
}

DecorationComponentHost::~DecorationComponentHost() {
    lifetime_->host = nullptr;
    services_->detach(*this);
}

void* DecorationComponentHost::begin_mount() noexcept {
    return std::exchange(active_decoration, this);
}

void DecorationComponentHost::end_mount(void* previous) noexcept {
    active_decoration = static_cast<DecorationComponentHost*>(previous);
}

void DecorationComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

DecorationSnapshot DecorationComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        throw std::out_of_range("Decoration component is stale");
    }
    return {s->kind, s->content, s->layer, s->visual, s->affixed, s->running, s->phase, s->status, s->texts, s->images};
}

runtime::ComponentId DecorationComponentHost::create(DecorationKind kind, const LayoutStyle& style,
                                                     const Content& content) {
    auto& b = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = b.mount_component<DecorationState>();
    auto& s = b.state<DecorationState>(id);
    s.kind = kind;
    s.node = b.root(id);
    b.on_resource_cleanup(id, [this, id, node = s.node] {
        auto* current = services_->components().state<DecorationState>(id);
        if (current) {
            // Reentrant cancellation sees a dead ticket before any application code.
            if (current->ticket) {
                current->ticket->active = false;
            }
            auto fn = std::move(current->cancel);
            clear_tiles(*services_, *current);
            if (current->master) {
                services_->text().scene_service().destroy(*current->master);
            }
            current->subscription.reset();
            invoke_cancel(std::move(fn));
        }
        services_->interactions().set_input_blocked(id, false);
        services_->layout().remove_layout(node);
    });
    b.mount_slot(id, Content{[&] {
                     auto& build = runtime::require_component_build_context();
                     s.layer = build.mount_component<int>(0);
                     s.layer_node = build.root(s.layer);
                     services.layout().set_layout(s.layer_node, layout::BoxLayout{});
                     build.on_resource_cleanup(
                         s.layer, [&services, node = s.layer_node] { services.layout().remove_layout(node); });
                     build.mount_slot(s.layer, Content{[&] {
                                          auto& layer_build = runtime::require_component_build_context();
                                          s.content = layer_build.mount_component<int>(0);
                                          s.content_node = layer_build.root(s.content);
                                          services.layout().set_layout(s.content_node, layout::BoxLayout{});
                                          layer_build.on_resource_cleanup(s.content,
                                                                          [&services, node = s.content_node] {
                                                                              services.layout().remove_layout(node);
                                                                          });
                                          layer_build.mount_slot(s.content, content);
                                      }});
                 }});
    s.fragment = b.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    if (kind == DecorationKind::BorderBeam) {
        s.effect = services.surfaces().create_content_range(s.fragment, {});
        b.on_resource_cleanup(id,
                              [&services, effect = s.effect] { services.surfaces().destroy_content_range(effect); });
    }
    services.nodes().require(s.node).owns_child_translation = kind == DecorationKind::Affix;
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](auto& engine, auto, auto c) {
                                            auto& state = *services_->components().state<DecorationState>(id);
                                            return c.constrain(engine.measure_child(state.layer_node, c));
                                        },
                                        [this, id](auto& engine, auto, auto bounds) {
                                            auto& state = *services_->components().state<DecorationState>(id);
                                            engine.place_child(state.layer_node, bounds);
                                        }});
    runtime::connect_layout_style(b.scope(id), style, s.node, services.nodes(), services.dirty());
    const auto scope = services.components().theme_scope(id);
    s.subscription = scope->capture([this, id](auto) { refresh(id); },
                                    [scope, kind] {
                                        if (kind == DecorationKind::Watermark) {
                                            static_cast<void>(scope->watermark());
                                        }
                                        if (kind == DecorationKind::Affix) {
                                            static_cast<void>(scope->affix());
                                        }
                                        if (kind == DecorationKind::BorderBeam) {
                                            static_cast<void>(scope->border_beam());
                                            static_cast<void>(scope->map());
                                        }
                                    });
    mounted_.push_back(id);
    return id;
}

void DecorationComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        return;
    }
    services_->interactions().set_input_blocked(id, !s->visible || s->disabled);
    if (services_->components().set_branch_active(s->layer, s->visible)) {
        services_->mark_scene_structure_dirty();
    }
    services_->nodes().require(s->node).out_of_flow = !s->visible;
    services_->dirty().invalidate(s->node, dirty_geometry | runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout);
    if (s->ready) {
        gate(id);
    }
}

void DecorationComponentHost::mount(const AffixProps& p, const AffixContent& content) {
    if (!p.target_) {
        throw std::invalid_argument("Affix requires an explicit native scroll target");
    }
    const auto id = create(DecorationKind::Affix, p.layout_, Content{SlotContentAccess::function(content)});
    auto& b = runtime::require_component_build_context();
    auto* s = services_->components().state<DecorationState>(id);
    s->change = p.change_;
    if (!p.top_ && !p.bottom_) {
        s->top = 0;
    }
    auto connect = [&](const auto& prop, auto apply) {
        connect_prop(b.scope(id), prop, [this, id, apply](auto v) {
            if (auto* state = services_->components().state<DecorationState>(id)) {
                apply(*state, v);
                refresh(id);
            }
        });
    };
    connect(*p.target_, [](auto& state, AffixTarget v) {
        require_finite(v.x);
        require_finite(v.y);
        nonnegative(v.width);
        nonnegative(v.height);
        nonnegative(v.offset);
        require_finite(v.x + v.width);
        require_finite(v.y + v.height);
        if (v.attached && v.identity == 0) {
            throw std::invalid_argument("Affix target requires a nonzero identity");
        }
        state.target = v;
    });
    if (p.top_) {
        connect(*p.top_, [](auto& state, float v) {
            require_finite(v);
            state.top = v;
        });
    }
    if (p.bottom_) {
        connect(*p.bottom_, [](auto& state, float v) {
            require_finite(v);
            state.bottom = v;
        });
    }
    connect(p.visible_, [](auto& state, bool v) { state.visible = v; });
    connect(p.disabled_, [](auto& state, bool v) { state.disabled = v; });
    s = services_->components().state<DecorationState>(id);
    if (s) {
        s->ready = true;
        refresh(id);
    }
}

void DecorationComponentHost::mount(const BorderBeamProps& p, const BorderBeamContent& content) {
    const auto id = create(DecorationKind::BorderBeam, p.layout_, Content{SlotContentAccess::function(content)});
    auto& b = runtime::require_component_build_context();
    auto connect = [&](const auto& prop, auto apply) {
        connect_prop(b.scope(id), prop, [this, id, apply](auto v) {
            if (auto* s = services_->components().state<DecorationState>(id)) {
                apply(*s, v);
                refresh(id);
            }
        });
    };
    connect(p.duration_, [](auto& s, Duration v) {
        if (v.count_milliseconds() <= 0) {
            throw std::invalid_argument("BorderBeam duration must be positive");
        }
        s.duration = v;
    });
    connect(p.direction_, [](auto& s, BorderBeamDirection v) {
        if (v != BorderBeamDirection::Clockwise && v != BorderBeamDirection::Counterclockwise) {
            throw std::invalid_argument("Invalid BorderBeam direction");
        }
        s.direction = v;
    });
    connect(p.visible_, [](auto& s, bool v) { s.visible = v; });
    connect(p.disabled_, [](auto& s, bool v) { s.disabled = v; });
    if (auto* s = services_->components().state<DecorationState>(id)) {
        s->ready = true;
        refresh(id);
    }
}

void DecorationComponentHost::mount(const WatermarkProps& p, const WatermarkContent& content) {
    const auto id = create(DecorationKind::Watermark, p.layout_, Content{SlotContentAccess::function(content)});
    auto& b = runtime::require_component_build_context();
    auto* state = services_->components().state<DecorationState>(id);
    state->load = p.load_;
    state->failure = p.error_;
    auto connect = [&](const auto& prop, auto apply) {
        connect_prop(b.scope(id), prop, [this, id, apply](auto v) {
            if (auto* s = services_->components().state<DecorationState>(id)) {
                apply(*s, v);
                refresh(id);
            }
        });
    };
    connect(p.content_, [](auto& s, String v) { s.text = std::move(v); });
    connect(p.size_, [](auto& s, WatermarkSize v) {
        nonnegative(v.width);
        nonnegative(v.height);
        if (v.width == 0 || v.height == 0) {
            throw std::invalid_argument("Watermark size must be positive");
        }
        s.size = v;
    });
    connect(p.gap_, [](auto& s, WatermarkGap v) {
        nonnegative(v.x);
        nonnegative(v.y);
        s.gap = v;
    });
    if (p.offset_) {
        connect(*p.offset_, [](auto& s, WatermarkOffset v) {
            require_finite(v.x);
            require_finite(v.y);
            s.offset = v;
        });
    }
    connect(p.rotate_, [](auto& s, float v) {
        require_finite(v);
        s.rotate = std::remainder(v, 360.F);
    });
    connect(p.visible_, [](auto& s, bool v) { s.visible = v; });
    connect(p.disabled_, [](auto& s, bool v) { s.disabled = v; });
    connect_prop(b.scope(id), p.image_, [this, id](ImageSource v) {
        if (auto* s = services_->components().state<DecorationState>(id)) {
            s->source = std::move(v);
            s->pixels.reset();
            s->status = ImageStatus::Empty;
            s->needs_load = s->source.value.index() != 0;
            s->report = false;
        }
        cancel(id);
        refresh(id);
    });
    state = services_->components().state<DecorationState>(id);
    if (state) {
        state->ready = true;
        refresh(id);
    }
}

void DecorationComponentHost::cancel(runtime::ComponentId id) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        return;
    }
    if (s->ticket) {
        s->ticket->active = false;
    }
    s->ticket.reset();
    auto fn = std::move(s->cancel);
    if (s->status == ImageStatus::Loading) {
        s->needs_load = true;
        s->status = ImageStatus::Empty;
    }
    invoke_cancel(std::move(fn));
}

void DecorationComponentHost::start(runtime::ComponentId id) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s || !s->needs_load) {
        return;
    }
    s->needs_load = false;
    s->status = ImageStatus::Loading;
    auto ticket = std::make_shared<Ticket>();
    s->ticket = ticket;
    std::weak_ptr<Lifetime> weak = lifetime_;
    ImageCompletion complete = [weak, id, ticket](ImageDecodeResult result) {
        if (std::this_thread::get_id() != ticket->owner || !ticket->active || ticket->completed) {
            return false;
        }
        auto life = weak.lock();
        if (!life || !life->host) {
            return false;
        }
        auto& host = *life->host;
        auto* state = host.services_->components().state<DecorationState>(id);
        if (!state || state->ticket != ticket || !enabled(*host.services_, id, *state)) {
            return false;
        }
        ticket->completed = true;
        state->pixels = std::move(result.pixels);
        state->status = state->pixels ? ImageStatus::Ready : ImageStatus::Error;
        state->error =
            state->pixels
                ? ImageError{}
                : (result.error ? result.error
                                : ImageError{ImageErrorCode::loader_failure, "Watermark source returned no pixels"});
        state->report = true;
        host.refresh(id);
        return true;
    };
    const auto source = s->source;
    if (const auto* pixels = std::get_if<std::shared_ptr<const ImagePixels>>(&source.value)) {
        complete({*pixels, {}});
    } else if (const auto* encoded = std::get_if<std::shared_ptr<const EncodedImage>>(&source.value)) {
        complete(*encoded ? decode_image(**encoded) : ImageDecodeResult{});
    } else if (const auto* loader = std::get_if<std::shared_ptr<const ImageLoader>>(&source.value)) {
        ImageCancel fn;
        try {
            if (!*loader || !(*loader)->start) {
                complete({{}, {ImageErrorCode::loader_failure, "Watermark loader is empty"}});
            } else {
                fn = (*loader)->start(complete);
            }
        } catch (...) {
            complete({{}, {ImageErrorCode::loader_failure, "Watermark loader threw"}});
        }
        s = services_->components().state<DecorationState>(id);
        if (s && s->ticket == ticket && ticket->active && !ticket->completed) {
            s->cancel = std::move(fn);
        } else if (!ticket->completed) {
            invoke_cancel(std::move(fn));
        }
    }
}

void DecorationComponentHost::gate(runtime::ComponentId id) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        return;
    }
    if (s->kind == DecorationKind::BorderBeam) {
        const bool run = enabled(*services_, id, *s) && s->onscreen && services_->focus().state().window_active &&
                         services_->motion_preference() != animation::MotionPreference::reduced &&
                         services_->components().theme_scope(id)->snapshot().map().motion;
        if (s->running != run) {
            s->running = run;
            s->last = services_->animation_time();
            if (run) {
                s->resuming = s->has_run;
                s->has_run = true;
            }
        }
    }
    if (s->kind == DecorationKind::Watermark && !enabled(*services_, id, *s)) {
        s->report = false;
        cancel(id);
    }
}

void DecorationComponentHost::on_window_active(bool) {
    const auto ids = mounted_;
    for (auto id : ids) {
        gate(id);
    }
}

void DecorationComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        gate(id);
    }
}

void DecorationComponentHost::synchronize_auxiliary_motion() {
    const auto ids = mounted_;
    for (auto id : ids) {
        gate(id);
    }
}

void DecorationComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        gate(id);
        auto* s = services_->components().state<DecorationState>(id);
        if (!s || !s->ready || s->kind != DecorationKind::Watermark) {
            continue;
        }
        if (enabled(*services_, id, *s)) {
            start(id);
        }
        s = services_->components().state<DecorationState>(id);
        if (!s || !s->report) {
            continue;
        }
        s->report = false;
        const auto status = s->status;
        const auto error = s->error;
        const auto load = s->load;
        const auto failure = s->failure;
        try {
            if (status == ImageStatus::Ready && load) {
                load();
            }
            if (status == ImageStatus::Error && failure) {
                failure(error);
            }
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    }
}

void DecorationComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    viewport_ = viewport;
    window_clip_ = clip;
}

void DecorationComponentHost::synchronize_retained_translations() {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DecorationState>(id);
        if (s && s->kind == DecorationKind::Affix) {
            services_->enqueue_retained_translation(id, [this, id] { compose_affix(id); });
        }
    }
}

void DecorationComponentHost::compose_affix(runtime::ComponentId id) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        return;
    }
    const auto natural = world(services_->nodes().require(s->node));
    const auto target = s->target;
    const runtime::Rect target_clip =
        graphics::intersect_effect_bounds({target.x, target.y, target.width, target.height}, window_clip_);
    float y = natural.y;
    const bool eligible =
        enabled(*services_, id, *s) && target.attached && target_clip.width > 0 && target_clip.height > 0;
    // offsetTop has the same precedence as the pinned native mapping.
    if (eligible && s->top && natural.y < target.y + *s->top) {
        y = target.y + *s->top;
    } else if (eligible && s->bottom && natural.y + natural.height > target.y + target.height - *s->bottom) {
        y = target.y + target.height - *s->bottom - natural.height;
    }
    const bool fixed = eligible && y != natural.y;
    if (s->affixed != fixed) {
        s->affixed = fixed;
        s->notify = true;
    }
    auto& layer = services_->nodes().require(s->layer_node);
    layer.clip_boundary = fixed;
    layer.clip_content = fixed;
    layer.translation = {};
    if (services_->components().set_window_layer(
            s->layer, fixed ? std::optional<int>{services_->components().theme_scope(id)->snapshot().affix().z_index}
                            : std::nullopt)) {
        services_->mark_scene_structure_dirty();
    }
    // The layer is the viewport clip. The retained child keeps its measured size and identity.
    layer.bounds = fixed ? target_clip : natural;
    layer.layout_size = {layer.bounds.width, layer.bounds.height};
    s->visual = {natural.x, y, natural.width, natural.height};
    services_->layout().place_retained_child(s->content_node, s->visual);
    // onChange is delivered after layout, never from a callback that still borrows LayoutEngine nodes.
}

void DecorationComponentHost::watermark(runtime::ComponentId id, runtime::Size viewport, runtime::Rect document_clip) {
    auto* s = services_->components().state<DecorationState>(id);
    if (!s) {
        return;
    }
    auto& scene = services_->text().scene_service();
    const auto bounds = world(services_->nodes().require(s->node));
    s->visual = bounds;
    auto clip = graphics::intersect_effect_bounds(bounds, services_->component_clip(id, viewport, document_clip));
    const bool use_image = s->source.value.index() != 0;
    if (!s->visible || s->disabled || !services_->components().branch_active(id) || clip.width <= 0 ||
        clip.height <= 0 || (use_image ? !s->pixels : s->text.empty())) {
        clear_tiles(*services_, *s);
        services_->scene_composer().set_fragment(s->fragment, {}, {}, clip);
        return;
    }
    const auto token = services_->components().theme_scope(id)->snapshot().watermark();
    runtime::Size mark{s->size.width, s->size.height};
    if (!use_image) {
        runtime::SemanticTypography typography;
        typography.font_family = token.font_family;
        typography.font_size = token.font_size;
        typography.line_height = token.line_height;
        const auto fonts = services_->text().resolve_fonts(typography);
        if (!s->master) {
            s->master = scene.create(s->node, s->text, fonts,
                                     static_cast<std::uint32_t>(std::max(1.F, std::round(token.font_size))),
                                     {token.line_height, s->size.width});
        }
        scene.set_content(*s->master, s->text);
        scene.set_font_chain(*s->master, fonts);
        scene.set_pixel_size(*s->master, static_cast<std::uint32_t>(std::max(1.F, std::round(token.font_size))));
        scene.set_line_height(*s->master, token.line_height);
        if (!scene.synchronize_measurement(*s->master, s->size.width)) {
            throw std::runtime_error("Watermark text measurement failed");
        }
        const auto& measurement = scene.text_state(*s->master).measurement();
        mark = {measurement.width, measurement.height};
    }
    const double dx = double(s->size.width) + s->gap.x;
    const double dy = double(s->size.height) + s->gap.y;
    const auto offset = s->offset.value_or(WatermarkOffset{s->gap.x / 2, s->gap.y / 2});
    const double margin = std::hypot(std::max(mark.width, s->size.width), std::max(mark.height, s->size.height));
    const double xbase = double(bounds.x) + std::remainder(double(offset.x), dx);
    const double ybase = double(bounds.y) + std::remainder(double(offset.y), dy);
    const double first_x = std::floor((clip.x - xbase - margin) / dx);
    const double first_y = std::floor((clip.y - ybase - margin) / dy);
    const double columns = std::ceil((clip.x + clip.width - xbase + margin) / dx) - first_x + 1;
    const double rows = std::ceil((clip.y + clip.height - ybase + margin) / dy) - first_y + 1;
    if (!std::isfinite(columns * rows) || columns * rows > 4096) {
        throw std::length_error("Watermark visible tile count exceeds 4096; increase gap or mark size");
    }
    const auto count = static_cast<std::size_t>(columns * rows);
    if (use_image) {
        for (auto text : s->texts) {
            scene.destroy(text);
        }
        s->texts.clear();
    } else {
        for (auto image : s->images) {
            services_->images().destroy(image);
        }
        s->images.clear();
    }
    while (s->texts.size() > (use_image ? 0 : count)) {
        scene.destroy(s->texts.back());
        s->texts.pop_back();
    }
    while (s->images.size() > (use_image ? count : 0)) {
        services_->images().destroy(s->images.back());
        s->images.pop_back();
    }
    const auto opacity = services_->nodes().content_opacity(s->node) * services_->nodes().paint_opacity(s->node);
    for (std::size_t index = 0; index < count; ++index) {
        const auto column = index % static_cast<std::size_t>(columns);
        const auto row = index / static_cast<std::size_t>(columns);
        const float x = static_cast<float>(xbase + (first_x + double(column)) * dx);
        const float y = static_cast<float>(ybase + (first_y + double(row)) * dy);
        if (use_image) {
            const auto r = fit_image({x, y, s->size.width, s->size.height},
                                     {float(s->pixels->width()), float(s->pixels->height())}, ImageFit::Contain);
            graphics::ImageInstance image{s->pixels, r,
                                          clip,      {x + s->size.width / 2, y + s->size.height / 2},
                                          s->rotate, opacity * token.image_opacity};
            if (index >= s->images.size()) {
                s->images.push_back(services_->images().create(image));
            } else {
                services_->images().update(s->images[index], image);
            }
        } else {
            if (index >= s->texts.size()) {
                s->texts.push_back(scene.create_view(*s->master, s->node));
            }
            const auto text = s->texts[index];
            scene.set_color(text, {token.color.red(), token.color.green(), token.color.blue(), token.color.alpha()});
            scene.set_content_opacity(text, opacity);
            const runtime::Point origin{x + (s->size.width - mark.width) / 2, y + (s->size.height - mark.height) / 2};
            scene.set_transform(text, {{mark.width / 2, mark.height / 2}, s->rotate});
            const graphics::GlyphPlacement placement{
                origin,
                viewport,
                clip,
                {},
                {token.color.red(), token.color.green(), token.color.blue(), token.color.alpha()},
                opacity};
            if (!scene.synchronize(text, placement)) {
                throw std::runtime_error("Watermark text rasterization failed");
            }
        }
    }
}

void DecorationComponentHost::beam(runtime::ComponentId id, runtime::Size viewport, runtime::Rect document_clip) {
    auto& s = *services_->components().state<DecorationState>(id);
    const auto bounds = world(services_->nodes().require(s.node));
    s.visual = bounds;
    const auto clip = services_->component_clip(id, viewport, document_clip);
    const auto visible = graphics::intersect_effect_bounds(bounds, clip);
    const auto token = services_->components().theme_scope(id)->snapshot().border_beam();
    s.onscreen = visible.width > 0 && visible.height > 0 && bounds.width > token.width && bounds.height > token.width;
    std::vector<graphics::RoundedEffectInstance> effects;
    if (s.visible && !s.disabled && services_->components().branch_active(id) && s.onscreen) {
        const auto width = std::min(token.width, std::min(bounds.width, bounds.height) / 2);
        const auto radius = std::min(token.radius, std::min(bounds.width, bounds.height) / 2);
        const graphics::LogicalRoundedRect centerline{
            {bounds.x + width / 2, bounds.y + width / 2, bounds.width - width, bounds.height - width},
            std::max(0.F, radius - width / 2)};
        const auto perimeter = graphics::rounded_rect_path_sample({}, centerline).length;
        if (perimeter > 0) {
            const auto length = std::min(token.length, perimeter * .75F);
            // One dash and one gap make one traveling segment, including across the closing seam.
            const auto phase = static_cast<float>(-s.phase * perimeter);
            effects.push_back(graphics::make_dashed_outline_effect(
                {bounds, radius}, width, {length, perimeter - length, phase}, token.color,
                services_->nodes().content_opacity(s.node), {}, graphics::EffectClip{1, clip}));
        }
    }
    services_->surfaces().update_content_effects(s.effect, effects);
    gate(id);
}

void DecorationComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DecorationState>(id);
        if (!s) {
            continue;
        }
        if (s->kind == DecorationKind::Watermark) {
            watermark(id, viewport, clip);
        } else if (s->kind == DecorationKind::BorderBeam) {
            beam(id, viewport, clip);
        } else if (s->notify) {
            const auto callback = s->change;
            const auto fixed = s->affixed;
            s->notify = false;
            if (callback) {
                try {
                    callback(fixed);
                } catch (...) {
                    services_->retire_failed_component(id);
                    throw;
                }
            }
        }
    }
}

bool DecorationComponentHost::synchronize_auxiliary_fragments() {
    auto& scene = services_->text().scene_service();
    for (auto id : mounted_) {
        auto* s = services_->components().state<DecorationState>(id);
        if (!s || s->kind != DecorationKind::Watermark) {
            continue;
        }
        std::vector<graphics::SceneDrawCommand> commands;
        for (auto text : s->texts) {
            for (const auto& draw : scene.primitive(text).draw_ranges) {
                commands.push_back(
                    {graphics::SceneDrawKind::glyph, draw.instances.first, draw.instances.count, draw.atlas_page});
            }
        }
        for (auto image : s->images) {
            commands.push_back({graphics::SceneDrawKind::image, image.index, 1, image.generation});
        }
        services_->scene_composer().set_fragment(s->fragment, commands);
    }
    return false;
}

std::size_t DecorationComponentHost::tick_auxiliary(animation::AnimationTime now) {
    std::size_t updates{};
    const auto ids = mounted_;
    for (auto id : ids) {
        gate(id);
        auto* s = services_->components().state<DecorationState>(id);
        if (!s || !s->running) {
            continue;
        }
        const auto time = std::max(s->last, now);
        if (s->resuming) {
            // The host clock may have stopped pumping while the window/page was inactive.
            s->last = time;
            s->resuming = false;
            continue;
        }
        const double delta =
            double((time - s->last).count_microseconds()) / (double(s->duration.count_milliseconds()) * 1000);
        s->phase = std::fmod(s->phase + (s->direction == BorderBeamDirection::Clockwise ? delta : -delta), 1.0);
        if (s->phase < 0) {
            s->phase += 1;
        }
        s->last = time;
        services_->dirty().invalidate(s->node, dirty_geometry);
        ++updates;
    }
    return updates;
}

std::optional<animation::AnimationTime> DecorationComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DecorationState>(id);
        if (!s || !s->running) {
            continue;
        }
        const auto time = s->last + animation::AnimationDuration::microseconds(16667);
        if (!next || time < *next) {
            next = time;
        }
    }
    return next;
}
} // namespace ryn::detail

namespace ryn {
void Affix(AffixProps p, AffixContent content) {
    if (!detail::active_decoration) {
        throw std::logic_error("Affix requires WindowComponentServices");
    }
    detail::active_decoration->mount(p, content);
}

void Watermark(WatermarkProps p, WatermarkContent content) {
    if (!detail::active_decoration) {
        throw std::logic_error("Watermark requires WindowComponentServices");
    }
    detail::active_decoration->mount(p, content);
}

void BorderBeam(BorderBeamProps p, BorderBeamContent content) {
    if (!detail::active_decoration) {
        throw std::logic_error("BorderBeam requires WindowComponentServices");
    }
    detail::active_decoration->mount(p, content);
}
} // namespace ryn
