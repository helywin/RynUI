#include "component/anchor_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ryn::detail {
namespace {
thread_local AnchorComponentHost* active_anchor{};
constexpr auto geometry_dirty = runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest;

struct AnchorRow final {
    AnchorItem item;
    std::size_t depth{};
    bool ancestor_visible{true};
    bool ancestor_disabled{};
    runtime::ComponentId component;
    runtime::NodeId node;
    std::unique_ptr<Scope> bindings;
    Signal<String> title{String{}};
    Signal<bool> disabled{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    float position{};
    std::optional<float> target_offset;
    bool visible{true};
    bool item_disabled{};
    runtime::Size size;

    explicit AnchorRow(AnchorItem value) : item(std::move(value)) {}
};

struct AnchorState final {
    runtime::NodeId node;
    runtime::ComponentId content;
    runtime::NodeId content_node;
    component::RetainedSurfaceId ink_surface;
    std::vector<std::shared_ptr<AnchorRow>> rows;
    std::vector<AnchorItem> pending;
    bool dirty_items{true};
    std::uint64_t items_revision{};
    bool reconciling{};
    bool disabled{};
    bool visible{true};
    bool affix{true};
    bool affixed{};
    bool ink{};
    bool controlled{};
    bool emitting_scroll{};
    AnchorDirection direction{AnchorDirection::Vertical};
    AnchorScrollState scroll;
    std::optional<AnchorScrollViewport> scroll_viewport;
    float offset_top{};
    std::optional<float> target_offset;
    float bounds{5};
    String active;
    String requested;
    Duration duration{Duration::milliseconds(450)};
    std::optional<animation::AnimationTime> started;
    float from{};
    float destination{};
    std::optional<float> last_request;
    float progress{};
    float navigation_offset{};
    float navigation_maximum{};
    std::optional<String> reveal;
    std::uint64_t generation{};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::function<void(float)> scroll_to;
    std::function<void(AnchorClickEvent&)> click;
    std::function<void(String)> change;
};

void nonnegative(float value) {
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument("Anchor requires finite non-negative logical dimensions");
    }
}

void offset_value(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Anchor offset must be finite");
    }
}

bool available(WindowComponentServices& services, runtime::ComponentId id, const AnchorState& s) {
    return s.visible && !s.disabled && services.components().branch_active(id) &&
           services.interactions().component_active(id) && services.focus().state().window_active;
}
} // namespace

AnchorComponentHost::AnchorComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

AnchorComponentHost::~AnchorComponentHost() {
    services_->detach(*this);
}

void* AnchorComponentHost::begin_mount() noexcept {
    return std::exchange(active_anchor, this);
}

void AnchorComponentHost::end_mount(void* previous) noexcept {
    active_anchor = static_cast<AnchorComponentHost*>(previous);
}

void AnchorComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

AnchorSnapshot AnchorComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<AnchorState>(id);
    if (!s) {
        throw std::out_of_range("Anchor component is stale");
    }
    AnchorSnapshot result{s->active, {}, {}, s->started.has_value(), s->affixed};
    for (const auto& row : s->rows) {
        result.keys.push_back(row->item.key_);
        result.items.push_back(row->component);
    }
    return result;
}

void AnchorComponentHost::validate_items(const std::vector<AnchorItem>& items) const {
    std::vector<String> keys;
    const auto visit = [&](auto&& self, const auto& values, std::size_t depth) -> void {
        if (depth > 128) {
            throw std::invalid_argument("Anchor hierarchy exceeds 128 levels");
        }
        for (const auto& item : values) {
            if (item.key_.empty() || std::ranges::find(keys, item.key_) != keys.end()) {
                throw std::invalid_argument("Anchor keys must be non-empty and unique");
            }
            keys.push_back(item.key_);
            nonnegative(read_prop(item.position_));
            if (item.target_offset_) {
                offset_value(read_prop(*item.target_offset_));
            }
            self(self, item.children_, depth + 1);
        }
    };
    visit(visit, items, 0);
}

void AnchorComponentHost::mount(const AnchorProps& props) {
    validate_items(read_prop(props.items_));
    if (!props.on_scroll_) {
        throw std::invalid_argument("Anchor requires an explicit scroll target callback");
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<AnchorState>();
    auto& s = build.state<AnchorState>(id);
    s.node = build.root(id);
    s.scroll_to = props.on_scroll_;
    s.click = props.on_click_;
    s.change = props.on_change_;
    s.controlled = props.active_.has_value();
    services.nodes().require(s.node).clip_content = false;
    build.mount_slot(id, Content{[&] {
                         auto& child = runtime::require_component_build_context();
                         s.content = child.mount_component<int>(0);
                         s.content_node = child.root(s.content);
                         services.nodes().require(s.content_node).clip_content = true;
                         services.layout().set_layout(s.content_node, layout::BoxLayout{});
                         child.on_resource_cleanup(
                             s.content, [&services, node = s.content_node] { services.layout().remove_layout(node); });
                         const auto fragment =
                             child.register_scene_fragment(s.content, runtime::SceneFragmentPlacement::after_children);
                         s.ink_surface = services.surfaces().create_content_range(fragment, {});
                         child.on_resource_cleanup(s.content, [&services, surface = s.ink_surface] {
                             services.surfaces().destroy_content_range(surface);
                         });
                     }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](auto& e, auto, auto c) { return measure(id, e, c); },
                                        [this, id](auto& e, auto, auto b) { place(id, e, b); }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    mounted_.push_back(id);
    auto connect = [&](const auto& prop, auto apply, bool geometry = false) {
        connect_prop(build.scope(id), prop, [this, id, apply, geometry](auto value) {
            if (auto* state = services_->components().state<AnchorState>(id)) {
                apply(*state, value);
                refresh(id, geometry);
                track(id);
            }
        });
    };
    connect(
        props.direction_,
        [](auto& state, AnchorDirection value) {
            if (value != AnchorDirection::Vertical && value != AnchorDirection::Horizontal) {
                throw std::invalid_argument("Invalid Anchor direction");
            }
            state.direction = value;
        },
        true);
    connect(
        props.offset_top_,
        [](auto& state, float value) {
            nonnegative(value);
            state.offset_top = value;
        },
        true);
    if (props.target_offset_) {
        connect(*props.target_offset_, [](auto& state, float value) {
            offset_value(value);
            state.target_offset = value;
            state.started.reset();
            ++state.generation;
        });
    }
    if (props.scroll_viewport_) {
        connect(
            *props.scroll_viewport_,
            [](auto& state, AnchorScrollViewport value) {
                offset_value(value.top);
                nonnegative(value.height);
                if (!std::isfinite(value.top + value.height)) {
                    throw std::invalid_argument("Anchor scroll viewport exceeds finite dimensions");
                }
                state.scroll_viewport = value;
            },
            true);
    }
    connect(props.bounds_, [](auto& state, float value) {
        nonnegative(value);
        state.bounds = value;
    });
    connect(props.affix_, [](auto& state, bool value) { state.affix = value; }, true);
    connect(props.ink_, [](auto& state, bool value) { state.ink = value; });
    connect(props.disabled_, [](auto& state, bool value) { state.disabled = value; });
    connect(props.visible_, [](auto& state, bool value) { state.visible = value; }, true);
    connect(props.duration_, [](auto& state, Duration value) { state.duration = value; });
    if (props.active_) {
        connect_prop(build.scope(id), *props.active_, [this, id](String value) {
            if (auto* state = services_->components().state<AnchorState>(id)) {
                state->active = std::move(value);
                refresh(id, false);
            }
        });
    }
    connect_prop(build.scope(id), props.scroll_, [this, id](AnchorScrollState value) {
        nonnegative(value.offset);
        nonnegative(value.extent);
        nonnegative(value.viewport);
        auto* state = services_->components().state<AnchorState>(id);
        if (!state) {
            return;
        }
        // A differing host acknowledgement means user input or an external scroll superseded this motion.
        if (state->started && std::abs(value.offset - state->last_request.value_or(state->scroll.offset)) > .5F) {
            state->started.reset();
            ++state->generation;
        }
        if (state->scroll.extent != value.extent || state->scroll.viewport != value.viewport) {
            state->started.reset();
            ++state->generation;
        }
        state->scroll = value;
        if (!state->started && !state->emitting_scroll) {
            track(id);
        }
        if (services_->components().contains(id)) {
            services_->dirty().invalidate(services_->components().root(id), geometry_dirty);
        }
    });
    connect_prop(build.scope(id), props.items_, [this, id](std::vector<AnchorItem> items) {
        validate_items(items);
        if (auto* state = services_->components().state<AnchorState>(id)) {
            ++state->items_revision;
            state->pending = std::move(items);
            state->dirty_items = true;
            services_->request_frame();
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::anchor_colors, &theme_runtime::ThemeScope::anchor_metrics,
                               &theme_runtime::ThemeScope::anchor_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](auto) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    reconcile(id, &build);
}

void AnchorComponentHost::reconcile(runtime::ComponentId id, runtime::ComponentBuildContext* initial) {
    auto& services = *services_;
    auto* s = services.components().state<AnchorState>(id);
    if (!s || !s->dirty_items || s->reconciling) {
        return;
    }

    struct Entry {
        AnchorItem item;
        std::size_t depth;
        bool visible;
        bool disabled;
    };

    std::vector<Entry> entries;
    const auto flatten = [&](auto&& self, const auto& items, std::size_t depth, bool visible, bool disabled) -> void {
        if (depth > 128) {
            throw std::invalid_argument("Anchor hierarchy exceeds 128 levels");
        }
        for (const auto& item : items) {
            if (item.key_.empty() ||
                std::ranges::any_of(entries, [&](const auto& e) { return e.item.key_ == item.key_; })) {
                throw std::invalid_argument("Anchor keys must be non-empty and unique");
            }
            nonnegative(read_prop(item.position_));
            if (item.target_offset_) {
                offset_value(read_prop(*item.target_offset_));
            }
            entries.push_back({item, depth, visible, disabled});
            self(self, item.children_, depth + 1, visible && read_prop(item.visible_),
                 disabled || read_prop(item.disabled_));
        }
    };
    flatten(flatten, s->pending, 0, true, false);
    const auto items_revision = s->items_revision;
    s->reconciling = true;
    const auto old = s->rows;
    std::vector<std::shared_ptr<AnchorRow>> next;
    std::vector<runtime::ComponentId> added;
    try {
        for (const auto& entry : entries) {
            auto found = std::ranges::find_if(old, [&](const auto& row) {
                return row->item.key_ == entry.item.key_ && row->item.rich_title_ == entry.item.rich_title_;
            });
            std::shared_ptr<AnchorRow> row;
            if (found != old.end()) {
                row = *found;
            } else {
                row = std::make_shared<AnchorRow>(entry.item);
                const auto content = Content{[&, row] {
                    auto& build = runtime::require_component_build_context();
                    row->component = build.mount_component<int>(0);
                    row->node = build.root(row->component);
                    services.nodes().require(row->node).clip_content = true;
                    services.layout().set_layout(row->node, layout::BoxLayout{});
                    build.on_resource_cleanup(row->component, [&services, node = row->node, row] {
                        row->bindings.reset();
                        services.layout().remove_layout(node);
                    });
                    build.mount_slot(
                        row->component, Content{[this, id, row] {
                            Theme(ThemeProps{}.config(row->theme), ThemeContent{[this, id, row] {
                                      Button(ButtonProps{}
                                                 .type(ButtonType::Link)
                                                 .wave(false)
                                                 .disabled(row->disabled)
                                                 .onClick([this, id, key = row->item.key_] { activate(id, key); }),
                                             ButtonContent{[row] {
                                                 if (row->item.rich_title_) {
                                                     SlotContentAccess::function (*row->item.rich_title_)();
                                                 } else {
                                                     Text(TextProps{}.content(row->title));
                                                 }
                                             }});
                                  }});
                        }});
                    for (auto hit : services.interactions().declaration_order()) {
                        const auto* record = services.interactions().find(hit);
                        if (!record) {
                            continue;
                        }
                        bool owns = false;
                        for (auto parent = std::optional{record->component}; parent;
                             parent = services.components().parent(*parent)) {
                            if (*parent == row->component) {
                                owns = true;
                                break;
                            }
                        }
                        if (!owns) {
                            continue;
                        }
                        auto handlers = record->focus_handlers ? *record->focus_handlers : input::FocusHandlers{};
                        const auto original = handlers.state_changed;
                        handlers.state_changed = [this, id, key = row->item.key_,
                                                  original](input::FocusPresentation p) {
                            if (original) {
                                original(p);
                            }
                            if (auto* state = services_->components().state<AnchorState>(id); state && p.focused) {
                                state->reveal = key;
                                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Layout |
                                                                               runtime::DirtyFlags::Geometry |
                                                                               runtime::DirtyFlags::HitTest);
                            }
                        };
                        services.interactions().set_focus_handlers(hit, std::move(handlers));
                    }
                }};
                if (initial) {
                    initial->mount_slot(s->content, content);
                } else {
                    services.append_slot(s->content, content);
                }
                added.push_back(row->component);
            }
            if (!services.components().contains(id)) {
                return;
            }
            row->depth = entry.depth;
            row->ancestor_visible = entry.visible;
            row->ancestor_disabled = entry.disabled;
            if (!(row->item == entry.item) || !row->bindings) {
                row->bindings = std::make_unique<Scope>();
                row->item = entry.item;
                const auto weak = std::weak_ptr{row};
                const auto update = [this, id, weak](auto apply) {
                    return [this, id, weak, apply](auto value) {
                        if (auto row = weak.lock(); row && services_->components().contains(id)) {
                            apply(*row, std::move(value));
                            refresh(id, true);
                            track(id);
                        }
                    };
                };
                connect_prop(*row->bindings, entry.item.title_,
                             update([](auto& r, String value) { r.title.set(std::move(value)); }));
                connect_prop(*row->bindings, entry.item.position_, update([](auto& r, float value) {
                    nonnegative(value);
                    r.position = value;
                }));
                connect_prop(*row->bindings, entry.item.visible_,
                             update([](auto& r, bool value) { r.visible = value; }));
                connect_prop(*row->bindings, entry.item.disabled_,
                             update([](auto& r, bool value) { r.item_disabled = value; }));
                row->target_offset.reset();
                if (entry.item.target_offset_) {
                    connect_prop(*row->bindings, *entry.item.target_offset_, update([](auto& r, float value) {
                        offset_value(value);
                        r.target_offset = value;
                    }));
                }
            }
            next.push_back(row);
        }
        s = services.components().state<AnchorState>(id);
        if (!s) {
            return;
        }
        s->rows = std::move(next);
        s->dirty_items = s->items_revision != items_revision;
        s->reconciling = false;
        s->started.reset();
        ++s->generation;
        for (const auto& row : old) {
            if (std::ranges::find(s->rows, row) == s->rows.end()) {
                services.destroy(row->component);
                s = services.components().state<AnchorState>(id);
                if (!s) {
                    return;
                }
            }
        }
        refresh(id, true);
        track(id);
    } catch (...) {
        for (auto child : added) {
            services.retire_failed_component(child);
        }
        if (auto* state = services.components().state<AnchorState>(id)) {
            state->reconciling = false;
            state->dirty_items = false;
        }
        throw;
    }
}

void AnchorComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<AnchorState>(id);
    if (!s || s->reconciling) {
        return;
    }
    if (geometry && s->started) {
        s->started.reset();
        ++s->generation;
    }
    const bool shown = s->visible && !s->rows.empty();
    services_->nodes().require(s->node).out_of_flow = !shown;
    if (services_->components().set_branch_active(id, shown)) {
        services_->mark_scene_structure_dirty();
    }
    if (!available(*services_, id, *s)) {
        s->started.reset();
        ++s->generation;
    }
    const auto token = services_->components().theme_scope(id)->snapshot().anchor();
    const auto rows = s->rows;
    std::vector<bool> visible_stack;
    std::vector<bool> disabled_stack;
    for (const auto& row : rows) {
        visible_stack.resize(row->depth, true);
        disabled_stack.resize(row->depth, false);
        row->ancestor_visible = std::ranges::all_of(visible_stack, [](bool v) { return v; });
        row->ancestor_disabled = std::ranges::any_of(disabled_stack, [](bool v) { return v; });
        visible_stack.push_back(row->visible);
        disabled_stack.push_back(row->item_disabled);
        const bool visible =
            row->visible && row->ancestor_visible && (s->direction == AnchorDirection::Vertical || row->depth == 0);
        const bool disabled = s->disabled || row->item_disabled || row->ancestor_disabled;
        services_->nodes().require(row->node).out_of_flow = !visible;
        if (services_->components().set_branch_active(row->component, visible)) {
            services_->mark_scene_structure_dirty();
        }
        row->disabled.set(disabled);
        if (!services_->components().contains(id)) {
            return;
        }
        ThemeConfig config;
        config.seed.control_height = dp(token.metrics.line_height + 2 * token.metrics.padding_block);
        config.seed.font_size = dp(token.metrics.font_size);
        config.text.tokens.font_family = token.metrics.font_family;
        config.text.tokens.line_height = dp(token.metrics.line_height);
        config.alias.color_focus_outline = Color(0, 0, 0, 0);
        config.seed.motion = false;
        config.alias.color_text_disabled = token.colors.disabled;
        config.button.tokens.border_radius = dp(token.metrics.radius);
        config.button.tokens.padding_inline = dp(0);
        config.button.tokens.border_width = dp(0);
        config.button.tokens.link_color = s->active == row->item.key_ ? token.colors.current : token.colors.item;
        config.button.tokens.link_hover_color = token.colors.hover;
        config.button.tokens.link_active_color = token.colors.current;
        config.button.tokens.link_hover_background = token.colors.hover_background;
        row->theme.set(config);
        if (!services_->components().contains(id)) {
            return;
        }
        services_->set_input_blocked(row->component, disabled || !visible || !s->visible);
        s = services_->components().state<AnchorState>(id);
        if (!s) {
            return;
        }
    }
    services_->dirty().invalidate(s->node, geometry ? geometry_dirty : runtime::DirtyFlags::Material);
}

void AnchorComponentHost::select(runtime::ComponentId id, String key, bool notify) {
    auto* s = services_->components().state<AnchorState>(id);
    if (!s) {
        return;
    }
    if (s->requested == key && (s->controlled || s->active == key)) {
        return;
    }
    s->requested = key;
    s->reveal = key;
    if (!s->controlled) {
        s->active = key;
    }
    const auto callback = s->change;
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    if (notify && callback) {
        callback(key);
    }
}

void AnchorComponentHost::track(runtime::ComponentId id) {
    auto* s = services_->components().state<AnchorState>(id);
    if (!s || s->reconciling || s->started) {
        return;
    }
    String key;
    float best = -std::numeric_limits<float>::infinity();
    for (const auto& row : s->rows) {
        if (!row->visible || !row->ancestor_visible || row->item_disabled || row->ancestor_disabled ||
            (s->direction == AnchorDirection::Horizontal && row->depth != 0)) {
            continue;
        }
        const float offset = row->target_offset.value_or(s->target_offset.value_or(s->offset_top));
        if (row->position - s->scroll.offset <= offset + s->bounds && row->position > best) {
            best = row->position;
            key = row->item.key_;
        }
    }
    const auto before = s->active;
    select(id, key, true);
    s = services_->components().state<AnchorState>(id);
    if (s && s->active != before) {
        refresh(id, false);
    }
}

void AnchorComponentHost::request_scroll(runtime::ComponentId id, float offset) {
    auto* s = services_->components().state<AnchorState>(id);
    if (!s) {
        return;
    }
    s->last_request = offset;
    const auto callback = s->scroll_to;
    s->emitting_scroll = true;
    try {
        callback(offset);
        if (auto* state = services_->components().state<AnchorState>(id)) {
            state->emitting_scroll = false;
        }
    } catch (...) {
        if (auto* state = services_->components().state<AnchorState>(id)) {
            state->started.reset();
            state->emitting_scroll = false;
            ++state->generation;
        }
        throw;
    }
}

void AnchorComponentHost::activate(runtime::ComponentId id, String key) {
    auto* s = services_->components().state<AnchorState>(id);
    if (!s || !available(*services_, id, *s)) {
        return;
    }
    const auto found = std::ranges::find_if(s->rows, [&](const auto& row) { return row->item.key_ == key; });
    if (found == s->rows.end()) {
        return;
    }
    const auto row = *found;
    if (!row->visible || !row->ancestor_visible || row->disabled.get()) {
        return;
    }
    const float maximum = std::max(0.0F, s->scroll.extent - s->scroll.viewport);
    AnchorClickEvent event{
        key, static_cast<std::size_t>(found - s->rows.begin()),
        std::clamp(row->position - row->target_offset.value_or(s->target_offset.value_or(s->offset_top)), 0.0F,
                   maximum)};
    const bool same_motion = s->started && s->requested == key && (!s->controlled || s->active == key);
    const auto callback = s->click;
    const auto generation = ++s->generation;
    if (!same_motion) {
        s->started.reset();
    }
    try {
        if (callback) {
            callback(event);
        }
    } catch (...) {
        if (auto* state = services_->components().state<AnchorState>(id)) {
            state->started.reset();
            ++state->generation;
        }
        throw;
    }
    s = services_->components().state<AnchorState>(id);
    if (!s || s->generation != generation || event.default_prevented || !available(*services_, id, *s)) {
        return;
    }
    if (same_motion) {
        return;
    }
    if (s->controlled && s->active != key) {
        s->requested = String{};
    }
    select(id, key, true);
    s = services_->components().state<AnchorState>(id);
    if (!s || s->generation != generation) {
        return;
    }
    s->last_request.reset();
    s->from = s->scroll.offset;
    s->destination = event.offset;
    s->progress = 0;
    const bool reduced = !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                           services_->motion_preference())
                              .enabled();
    if (reduced || s->duration.count_milliseconds() == 0 || s->from == s->destination) {
        request_scroll(id, event.offset);
    } else {
        s->started = services_->animation_time();
        services_->request_frame();
    }
    if (services_->components().contains(id)) {
        refresh(id, false);
    }
}

void AnchorComponentHost::prepare_auxiliary_layout() {
    for (const auto id : std::vector{mounted_}) {
        reconcile(id);
    }
}

runtime::Size AnchorComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                           layout::Constraints c) {
    auto& s = *services_->components().state<AnchorState>(id);
    const auto m = services_->components().theme_scope(id)->snapshot().anchor().metrics;
    float width{};
    float height{};
    for (const auto& row : s.rows) {
        if (services_->nodes().require(row->node).out_of_flow) {
            continue;
        }
        const float padding = m.padding + (s.direction == AnchorDirection::Vertical ? m.indent * row->depth : 0);
        row->size = engine.measure_child(
            row->node, {0,
                        (s.direction == AnchorDirection::Vertical ? std::max(0.0F, c.max_width - padding)
                                                                  : std::numeric_limits<float>::infinity()),
                        0, c.max_height});
        if (s.direction == AnchorDirection::Vertical) {
            width = std::max(width, padding + row->size.width);
            height += row->size.height;
        } else {
            width += padding + row->size.width;
            height = std::max(height, row->size.height);
        }
    }
    return c.constrain({width, height});
}

void AnchorComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<AnchorState>(id);
    services_->nodes().require(s.content_node).bounds = bounds;
    services_->nodes().require(s.content_node).layout_size = {bounds.width, bounds.height};
    const auto m = services_->components().theme_scope(id)->snapshot().anchor().metrics;
    const bool horizontal = s.direction == AnchorDirection::Horizontal;
    float extent{};
    for (const auto& row : s.rows) {
        if (services_->nodes().require(row->node).out_of_flow) {
            continue;
        }
        const float size = horizontal ? m.padding + row->size.width : row->size.height;
        if (s.reveal && row->item.key_ == *s.reveal) {
            const float viewport = horizontal ? bounds.width : bounds.height;
            if (extent < s.navigation_offset) {
                s.navigation_offset = extent;
            } else if (extent + size > s.navigation_offset + viewport) {
                s.navigation_offset = extent + size - viewport;
            }
        }
        extent += size;
    }
    s.navigation_maximum = std::max(0.0F, extent - (horizontal ? bounds.width : bounds.height));
    s.navigation_offset = std::clamp(s.navigation_offset, 0.0F, s.navigation_maximum);
    float x = bounds.x - (horizontal ? s.navigation_offset : 0);
    float y = bounds.y - (horizontal ? 0 : s.navigation_offset);
    for (const auto& row : s.rows) {
        if (services_->nodes().require(row->node).out_of_flow) {
            continue;
        }
        const float padding = std::min(bounds.width, m.padding + (horizontal ? 0 : m.indent * row->depth));
        if (!horizontal) {
            engine.place_child(row->node,
                               {bounds.x + padding, y, std::max(0.0F, bounds.width - padding), row->size.height});
            y += row->size.height;
        } else {
            engine.place_child(row->node, {x + padding, y, row->size.width, bounds.height});
            x += padding + row->size.width;
        }
    }
}

void AnchorComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<AnchorState>(id);
        if (!s) {
            continue;
        }
        const auto& root = services_->nodes().require(s->node);
        const auto natural = root.bounds;
        const auto translation = services_->nodes().require(s->content_node).translation;
        const float target_top = s->scroll_viewport ? s->scroll_viewport->top : clip.y;
        const float target_bottom =
            s->affix && s->scroll_viewport
                ? std::min(viewport.height, s->scroll_viewport->top + s->scroll_viewport->height)
                : viewport.height;
        const float top = std::clamp(target_top + s->offset_top, clip.y, std::max(clip.y, viewport.height));
        const float natural_y = natural.y + root.translation.y;
        const float y = s->affix ? std::max(natural_y, top) : natural_y;
        s->affixed = s->visible && y != natural_y;
        services_->nodes().require(s->content_node).clip_boundary = s->affixed;
        if (services_->components().set_window_layer(s->content, s->affixed ? std::optional<int>{1} : std::nullopt)) {
            services_->mark_scene_structure_dirty();
        }
        // The root remains the placeholder. Only the retained content is translated through normal layout.
        place(id, services_->layout(),
              {natural.x + root.translation.x - translation.x, y - translation.y, natural.width,
               std::min(natural.height, std::max(0.0F, target_bottom - y))});
        s->reveal.reset();
    }
}

void AnchorComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (const auto id : mounted_) {
        auto* s = services_->components().state<AnchorState>(id);
        if (!s) {
            continue;
        }
        const auto token = services_->components().theme_scope(id)->snapshot().anchor();
        const auto b = services_->nodes().require(s->content_node).bounds;
        const graphics::EffectClip effect_clip{1, services_->component_clip(s->content, viewport, clip)};
        std::vector<graphics::RoundedEffectInstance> effects;
        const float stroke = std::min(services_->components().theme_scope(id)->snapshot().seed().line_width * 2,
                                      std::min(b.width, b.height));
        if (s->visible && services_->components().branch_active(id) && stroke > 0) {
            const bool horizontal = s->direction == AnchorDirection::Horizontal;
            const runtime::Rect rail = horizontal ? runtime::Rect{b.x, b.y + b.height - stroke, b.width, stroke}
                                                  : runtime::Rect{b.x, b.y, stroke, b.height};
            const auto add = [&](runtime::Rect r, Color color) {
                auto fill = graphics::make_corner_fill_effects(
                    {r, 0}, {true, true, true, true}, color, services_->nodes().content_opacity(s->content_node),
                    services_->nodes().require(s->content_node).translation, effect_clip);
                effects.insert(effects.end(), fill.begin(), fill.end());
            };
            add(rail, token.colors.separator);
            for (const auto& row : s->rows) {
                if (row->item.key_ != s->active || services_->nodes().require(row->node).out_of_flow ||
                    (!s->affix && !s->ink)) {
                    continue;
                }
                const auto r = services_->nodes().require(row->node).bounds;
                add(horizontal ? runtime::Rect{r.x, b.y + b.height - stroke, r.width, stroke}
                               : runtime::Rect{b.x, r.y, stroke, r.height},
                    token.colors.current);
            }
        }
        const auto focused = services_->focus().state();
        const auto* focus_record = focused.focused ? services_->interactions().find(*focused.focused) : nullptr;
        if (focus_record && focused.focus_visible && token.effects.focus_width > 0) {
            for (const auto& row : s->rows) {
                bool owns = false;
                for (auto parent = std::optional{focus_record->component}; parent;
                     parent = services_->components().parent(*parent)) {
                    if (*parent == row->component) {
                        owns = true;
                        break;
                    }
                }
                if (owns) {
                    const auto r = services_->nodes().require(row->node).bounds;
                    if (r.width <= 0 || r.height <= 0 || row->disabled.get() ||
                        services_->nodes().require(row->node).out_of_flow) {
                        continue;
                    }
                    effects.push_back(graphics::make_outline_effect(
                        {r, std::min(token.metrics.radius, std::min(r.width, r.height) / 2)}, token.effects.focus_width,
                        token.effects.focus_offset, token.colors.focus, services_->nodes().content_opacity(row->node),
                        services_->nodes().require(row->node).translation, effect_clip));
                }
            }
        }
        services_->surfaces().update_content_effects(s->ink_surface, effects);
    }
}

bool AnchorComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (std::ranges::none_of(mounted_, [this](auto id) {
            const auto* s = services_->components().state<AnchorState>(id);
            return s && s->navigation_maximum > 0 && available(*services_, id, *s);
        })) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    for (auto owner = std::optional{target->component}; owner; owner = services_->components().parent(*owner)) {
        for (const auto id : mounted_) {
            auto* s = services_->components().state<AnchorState>(id);
            if (!s || *owner != s->content || !available(*services_, id, *s) || s->navigation_maximum <= 0) {
                continue;
            }
            const float delta =
                s->direction == AnchorDirection::Horizontal && event.delta_x != 0 ? event.delta_x : -event.delta_y;
            s->navigation_offset = std::clamp(s->navigation_offset + delta * 40, 0.0F, s->navigation_maximum);
            s->reveal.reset();
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                                                       runtime::DirtyFlags::HitTest);
            return true;
        }
        if (services_->components().is_window_layer_root(*owner)) {
            break;
        }
    }
    return false;
}

void AnchorComponentHost::on_input_boundary_changed() {
    for (const auto id : mounted_) {
        if (auto* s = services_->components().state<AnchorState>(id); s && !available(*services_, id, *s)) {
            s->started.reset();
            ++s->generation;
        }
    }
}

void AnchorComponentHost::on_window_active(bool active) {
    if (!active) {
        on_input_boundary_changed();
    }
}

std::size_t AnchorComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t count{};
    for (const auto id : std::vector{mounted_}) {
        auto* s = services_->components().state<AnchorState>(id);
        if (!s || !s->started) {
            continue;
        }
        if (!available(*services_, id, *s)) {
            s->started.reset();
            continue;
        }
        const bool reduced = !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                               services_->motion_preference())
                                  .enabled();
        const auto elapsed = time >= *s->started ? (time - *s->started).count_microseconds() : 0;
        s->progress = std::max(
            s->progress,
            reduced || s->duration.count_milliseconds() == 0
                ? 1.0F
                : std::clamp(static_cast<float>(elapsed / (1000.0 * s->duration.count_milliseconds())), 0.0F, 1.0F));
        const float value = s->destination + (s->from - s->destination) * std::pow(1 - s->progress, 3);
        const bool completed = s->progress == 1;
        const auto generation = s->generation;
        request_scroll(id, value);
        if (auto* state = services_->components().state<AnchorState>(id);
            state && completed && state->generation == generation) {
            state->started.reset();
        }
        ++count;
    }
    return count;
}

std::optional<animation::AnimationTime> AnchorComponentHost::next_auxiliary_deadline() const {
    for (const auto id : mounted_) {
        if (const auto* s = services_->components().state<AnchorState>(id); s && s->started) {
            return services_->animation_time() + animation::AnimationDuration::milliseconds(16);
        }
    }
    return {};
}
} // namespace ryn::detail

namespace ryn {
void Anchor(AnchorProps props) {
    if (!detail::active_anchor) {
        throw std::logic_error("Anchor requires window component services");
    }
    detail::active_anchor->mount(props);
}
} // namespace ryn
