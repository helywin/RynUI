#include "component/config_defaults.hpp"
#include "component/grid_component.hpp"

#include "component/responsive_layout.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {

constexpr auto grid_dirty = runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry |
                            runtime::DirtyFlags::HitTest;

struct ColState final {
    runtime::ComponentId row;
    ColOptions base;
    Responsive<ColOptions> responsive;
    ColOptions resolved;
    runtime::Rect frame;
    float half_gutter{};
};

struct GridItem final {
    runtime::ComponentId id;
    float width{};
    float height{};
    float offset{};
    float displacement{};
    float grow{};
    float shrink{};
    float minimum{};
    float maximum{std::numeric_limits<float>::infinity()};
    bool flexible{};
    bool frozen{};
};

struct GridLine final {
    std::size_t begin{};
    std::size_t end{};
    float height{};
    float used{};
};

struct RowState final {
    runtime::ComponentId id;
    Responsive<RowAlign> align{RowAlign::Top};
    Responsive<RowJustify> justify{RowJustify::Start};
    GridGutter gutter;
    Breakpoints breakpoints;
    bool wrap{true};
    bool rtl{};
    float window_width{};
    std::vector<runtime::ComponentId> columns;
    std::vector<GridItem> items;
    std::vector<GridLine> lines;
    std::uint64_t generation{};
};

template <typename T, typename Validate> void validate_responsive(const Responsive<T>& value, Validate validate) {
    validate(value.fallback());
    for (const auto& entry : value.overrides()) {
        if (entry) {
            validate(*entry);
        }
    }
}

void validate(const ColOptions& value) {
    for (const auto field : {value.span, value.offset, value.push, value.pull}) {
        if (field && (*field < 0 || *field > 24)) {
            throw std::invalid_argument("Grid span, offset, push and pull must be within 0..24");
        }
    }
}

void validate(RowAlign value) {
    if (value != RowAlign::Top && value != RowAlign::Middle && value != RowAlign::Bottom &&
        value != RowAlign::Stretch) {
        throw std::invalid_argument("Grid row alignment is invalid");
    }
}

void validate(RowJustify value) {
    if (value != RowJustify::Start && value != RowJustify::End && value != RowJustify::Center &&
        value != RowJustify::SpaceAround && value != RowJustify::SpaceBetween && value != RowJustify::SpaceEvenly) {
        throw std::invalid_argument("Grid row justification is invalid");
    }
}

void validate(const GridGutter& value) {
    const auto gap = [](LogicalLength length) {
        if (length.is_auto() || !std::isfinite(length.value()) || length.value() < 0) {
            throw std::invalid_argument("Grid gutter must be a finite non-negative logical length");
        }
    };
    validate_responsive(value.horizontal, gap);
    validate_responsive(value.vertical, gap);
}

void overlay(ColOptions& target, const ColOptions& source) {
    if (source.span) {
        target.span = source.span;
    }
    if (source.offset) {
        target.offset = source.offset;
    }
    if (source.order) {
        target.order = source.order;
    }
    if (source.push) {
        target.push = source.push;
    }
    if (source.pull) {
        target.pull = source.pull;
    }
    if (source.flex) {
        target.flex = source.flex;
    }
}

void refresh(WindowComponentServices& services, runtime::ComponentId id) {
    auto* row = services.components().state<RowState>(id);
    if (!row) {
        return;
    }
    services.dirty().invalidate(services.components().root(id), grid_dirty);
    const auto columns = row->columns;
    for (const auto col_id : columns) {
        row = services.components().state<RowState>(id);
        auto* col = services.components().state<ColState>(col_id);
        if (!row || !col) {
            continue;
        }
        auto resolved = col->base;
        overlay(resolved, col->responsive.fallback());
        col->responsive.visit(row->window_width, row->breakpoints,
                              [&](const ColOptions& options) { overlay(resolved, options); });
        col->resolved = resolved;
        const bool shown = resolved.span != 0;
        services.nodes().require(services.components().root(col_id)).out_of_flow = !shown;
        if (services.components().set_branch_active(col_id, shown)) {
            services.mark_scene_structure_dirty();
        }
        // This can deliver blur/capture cancellation and destroy the entire row.
        services.set_input_blocked(col_id, !shown);
    }
}

void distribute(std::vector<GridItem>& items, const GridLine& line, float available) {
    float used{};
    for (std::size_t index = line.begin; index < line.end; ++index) {
        used += items[index].offset + items[index].width;
    }
    const bool grow = used < available;
    float remaining = available - used;
    for (std::size_t pass = line.begin; pass < line.end && std::abs(remaining) > 0.0001F; ++pass) {
        float weight{};
        for (std::size_t index = line.begin; index < line.end; ++index) {
            const auto& item = items[index];
            if (item.flexible && !item.frozen) {
                weight += grow ? item.grow : item.shrink * item.width;
            }
        }
        if (weight <= 0) {
            break;
        }
        float consumed{};
        bool clamped{};
        for (std::size_t index = line.begin; index < line.end; ++index) {
            auto& item = items[index];
            if (!item.flexible || item.frozen) {
                continue;
            }
            const float part = grow ? item.grow : item.shrink * item.width;
            const float candidate = item.width + remaining * part / weight;
            const float width = std::clamp(candidate, item.minimum, item.maximum);
            consumed += width - item.width;
            item.width = width;
            if (width != candidate) {
                item.frozen = true;
                clamped = true;
            }
        }
        remaining -= consumed;
        if (!clamped) {
            break;
        }
    }
}

runtime::Size measure_row(WindowComponentServices& services, runtime::ComponentId id, layout::LayoutEngine& engine,
                          layout::Constraints limits) {
    auto& row = *services.components().state<RowState>(id);
    row.items.clear();
    row.lines.clear();
    const float gutter = row.gutter.horizontal.resolve(row.window_width, row.breakpoints).value();
    const float vertical = row.gutter.vertical.resolve(row.window_width, row.breakpoints).value();
    float width = limits.max_width;
    const bool bounded = std::isfinite(width);
    float natural{};
    for (const auto col_id : services.components().children(id)) {
        auto* col = services.components().state<ColState>(col_id);
        if (!col || col->row != id) {
            throw std::invalid_argument("Grid Row accepts only direct Col children");
        }
        if (col->resolved.span == 0) {
            continue;
        }
        col->half_gutter = gutter / 2;
        const auto node = services.components().root(col_id);
        const auto size = engine.measure_child(
            node, {0, bounded ? width + gutter : width, 0, std::numeric_limits<float>::infinity()});
        GridItem item;
        item.id = col_id;
        item.width = size.width;
        item.height = size.height;
        const auto& external = services.nodes().require(node).external_layout;
        const float margins = external.margin.left + external.margin.right;
        item.minimum = external.min_width.value_or(0) + margins;
        item.maximum = external.max_width.value_or(std::numeric_limits<float>::infinity()) + margins;
        item.maximum = std::max(item.minimum, item.maximum);
        if (const auto& flex = col->resolved.flex) {
            item.flexible = true;
            item.grow = flex->grow();
            item.shrink = flex->shrink();
            if (!flex->basis().is_auto()) {
                item.width = flex->basis().value() + margins;
            }
        } else if (bounded && col->resolved.span) {
            item.width = (width + gutter) * static_cast<float>(*col->resolved.span) / 24;
        } else {
            item.flexible = true;
            item.shrink = 1;
        }
        item.width = std::clamp(item.width, item.minimum, item.maximum);
        natural += item.width;
        row.items.push_back(item);
    }
    if (!bounded) {
        width = std::max(0.0F, natural - gutter);
    }
    const float available = width + gutter;
    std::stable_sort(row.items.begin(), row.items.end(), [&](const auto& first, const auto& second) {
        return services.components().state<ColState>(first.id)->resolved.order.value_or(0) <
               services.components().state<ColState>(second.id)->resolved.order.value_or(0);
    });
    GridLine line;
    for (std::size_t index = 0; index < row.items.size(); ++index) {
        auto& item = row.items[index];
        const auto& options = services.components().state<ColState>(item.id)->resolved;
        item.offset = available * static_cast<float>(options.offset.value_or(0)) / 24;
        item.displacement = available * static_cast<float>(options.push.value_or(0) - options.pull.value_or(0)) / 24;
        if (row.wrap && index != line.begin && line.used + item.offset + item.width > available + 0.001F) {
            line.end = index;
            row.lines.push_back(line);
            line = GridLine{.begin = index};
        }
        line.used += item.offset + item.width;
    }
    if (!row.items.empty()) {
        line.end = row.items.size();
        row.lines.push_back(line);
    }
    const auto align = row.align.resolve(row.window_width, row.breakpoints);
    float height{};
    for (auto& current : row.lines) {
        distribute(row.items, current, available);
        current.used = 0;
        for (std::size_t index = current.begin; index < current.end; ++index) {
            auto& item = row.items[index];
            const auto size =
                engine.measure_child(services.components().root(item.id),
                                     {0, available, 0, std::numeric_limits<float>::infinity()}, item.width);
            item.height = size.height;
            current.height = std::max(current.height, size.height);
            current.used += item.offset + item.width;
        }
        height += current.height;
    }
    if (row.lines.size() > 1) {
        height += vertical * static_cast<float>(row.lines.size() - 1);
    }
    const auto measured = limits.constrain({width, height});
    const float extra_height =
        row.lines.empty() ? 0 : std::max(0.0F, measured.height - height) / static_cast<float>(row.lines.size());
    for (auto& current : row.lines) {
        current.height += extra_height;
        if (align == RowAlign::Stretch) {
            for (std::size_t index = current.begin; index < current.end; ++index) {
                auto& item = row.items[index];
                const auto& external = services.nodes().require(services.components().root(item.id)).external_layout;
                if (!external.height) {
                    const float margin = external.margin.top + external.margin.bottom;
                    const float maximum = external.max_height.value_or(std::numeric_limits<float>::infinity()) + margin;
                    const float minimum = std::min(maximum, external.min_height.value_or(0) + margin);
                    const float stretched = std::clamp(current.height, minimum, maximum);
                    const auto size = engine.measure_child(services.components().root(item.id),
                                                           {0, available, 0, current.height}, item.width, stretched);
                    item.height = size.height;
                }
            }
        }
    }
    row.generation = engine.generation();
    return measured;
}

void place_row(WindowComponentServices& services, runtime::ComponentId id, layout::LayoutEngine& engine,
               runtime::Rect bounds) {
    auto& row = *services.components().state<RowState>(id);
    if (row.generation != engine.generation()) {
        throw std::logic_error("Grid placement requires the matching measurement generation");
    }
    const float gutter = row.gutter.horizontal.resolve(row.window_width, row.breakpoints).value();
    const float vertical = row.gutter.vertical.resolve(row.window_width, row.breakpoints).value();
    const float available = bounds.width + gutter;
    const auto justify = row.justify.resolve(row.window_width, row.breakpoints);
    const auto align = row.align.resolve(row.window_width, row.breakpoints);
    float y{};
    for (const auto& line : row.lines) {
        const float free = std::max(0.0F, available - line.used);
        const float count = static_cast<float>(line.end - line.begin);
        float x{};
        float gap{};
        switch (justify) {
        case RowJustify::Start:
            break;
        case RowJustify::End:
            x = free;
            break;
        case RowJustify::Center:
            x = free / 2;
            break;
        case RowJustify::SpaceBetween:
            gap = count > 1 ? free / (count - 1) : 0;
            break;
        case RowJustify::SpaceAround:
            gap = free / count;
            x = gap / 2;
            break;
        case RowJustify::SpaceEvenly:
            gap = free / (count + 1);
            x = gap;
            break;
        }
        for (std::size_t index = line.begin; index < line.end; ++index) {
            const auto& item = row.items[index];
            x += item.offset;
            float dy{};
            if (align == RowAlign::Middle) {
                dy = (line.height - item.height) / 2;
            } else if (align == RowAlign::Bottom) {
                dy = line.height - item.height;
            }
            float logical_x = x + item.displacement;
            if (row.rtl) {
                logical_x = available - logical_x - item.width;
            }
            auto& col = *services.components().state<ColState>(item.id);
            col.frame = {bounds.x - gutter / 2 + logical_x, bounds.y + y + dy, item.width, item.height};
            engine.place_child(services.components().root(item.id), col.frame, true, align == RowAlign::Stretch);
            x += item.width + gap;
        }
        y += line.height + vertical;
    }
}

} // namespace

struct GridPropsAccess final {
    static void mount_row(const RowProps& props, const RowContent& content) {
        auto& responsive = require_responsive_layout_host();
        auto& services = responsive.services();
        auto& build = runtime::require_component_build_context();
        validate_responsive(read_prop(props.align_), [](auto value) { validate(value); });
        validate_responsive(read_prop(props.justify_), [](auto value) { validate(value); });
        validate(read_prop(props.gutter_));
        const auto id = build.mount_component<RowState>();
        auto& state = build.state<RowState>(id);
        state.id = id;
        state.window_width = responsive.window_width().get();
        const auto node = build.root(id);
        services.layout().set_layout(node, layout::ComponentLayout{[&services, id](auto& engine, auto, auto limits) {
                                                                       return measure_row(services, id, engine, limits);
                                                                   },
                                                                   [&services, id](auto& engine, auto, auto bounds) {
                                                                       place_row(services, id, engine, bounds);
                                                                   }});
        build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
        runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
        auto& scope = build.scope(id);
        connect_prop(scope, props.align_, [&services, id](const auto& value) {
            validate_responsive(value, [](auto entry) { validate(entry); });
            services.components().state<RowState>(id)->align = value;
            refresh(services, id);
        });
        connect_prop(scope, props.justify_, [&services, id](const auto& value) {
            validate_responsive(value, [](auto entry) { validate(entry); });
            services.components().state<RowState>(id)->justify = value;
            refresh(services, id);
        });
        connect_prop(scope, props.gutter_, [&services, id](const auto& value) {
            validate(value);
            services.components().state<RowState>(id)->gutter = value;
            refresh(services, id);
        });
        connect_prop(scope, props.wrap_, [&services, id](bool value) {
            services.components().state<RowState>(id)->wrap = value;
            refresh(services, id);
        });
        connect_prop(scope, props.direction_, [&services, id](FlexDirection value) {
            if (value != FlexDirection::LeftToRight && value != FlexDirection::RightToLeft) {
                throw std::invalid_argument("Grid direction is invalid");
            }
            services.components().state<RowState>(id)->rtl = value == FlexDirection::RightToLeft;
            refresh(services, id);
        });
        connect_prop(scope, props.breakpoints_, [&services, id](const auto& value) {
            services.components().state<RowState>(id)->breakpoints = value;
            refresh(services, id);
        });
        connect_prop(scope, Prop<float>{responsive.window_width()}, [&services, id](float value) {
            services.components().state<RowState>(id)->window_width = value;
            refresh(services, id);
        });
        build.mount_slot(id, content);
        for (const auto child : services.components().children(id)) {
            if (!services.components().state<ColState>(child)) {
                throw std::invalid_argument("Grid Row accepts only direct Col children");
            }
        }
        refresh(services, id);
    }

    static void mount_col(const ColProps& props, const ColContent& content) {
        auto& services = require_responsive_layout_host().services();
        auto& build = runtime::require_component_build_context();
        auto* row = build.nearest_state<RowState>();
        if (!row) {
            throw std::logic_error("Grid Col requires a parent Row");
        }
        ColOptions base{.span = props.span_ ? std::optional{read_prop(*props.span_)} : std::nullopt,
                        .offset = read_prop(props.offset_),
                        .order = read_prop(props.order_),
                        .push = read_prop(props.push_),
                        .pull = read_prop(props.pull_),
                        .flex = props.flex_ ? std::optional{read_prop(*props.flex_)} : std::nullopt};
        validate(base);
        validate_responsive(read_prop(props.responsive_), [](const auto& value) { validate(value); });
        const auto row_id = row->id;
        const auto id = build.mount_component<ColState>();
        if (services.components().parent(id) != row_id) {
            throw std::invalid_argument("Grid Col must be a direct child of Row");
        }
        auto& state = build.state<ColState>(id);
        state.row = row_id;
        state.base = base;
        state.resolved = base;
        row->columns.push_back(id);
        const auto node = build.root(id);
        // The padding is resolved by Row before measuring any column.
        services.layout().set_layout(
            node, layout::ComponentLayout{
                      [&services, id](auto& engine, auto node, layout::Constraints limits) {
                          const float padding = services.components().state<ColState>(id)->half_gutter;
                          const float inner = std::max(0.0F, limits.max_width - 2 * padding);
                          runtime::Size size;
                          for (const auto child : services.nodes().require(node).children) {
                              if (!services.nodes().require(child).out_of_flow) {
                                  const auto measured = engine.measure_child(child, {0, inner, 0, limits.max_height});
                                  size.width = std::max(size.width, measured.width);
                                  size.height = std::max(size.height, measured.height);
                              }
                          }
                          return limits.constrain({size.width + 2 * padding, size.height});
                      },
                      [&services, id](auto& engine, auto node, runtime::Rect bounds) {
                          const float padding = services.components().state<ColState>(id)->half_gutter;
                          for (const auto child : services.nodes().require(node).children) {
                              if (!services.nodes().require(child).out_of_flow) {
                                  const auto size = services.nodes().require(child).layout_size;
                                  engine.place_child(child,
                                                     {bounds.x + padding, bounds.y,
                                                      std::min(size.width, std::max(0.0F, bounds.width - 2 * padding)),
                                                      std::min(size.height, bounds.height)});
                              }
                          }
                      }});
        build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
        runtime::connect_layout_style(build.scope(id), props.layout_, node, services.nodes(), services.dirty());
        auto& scope = build.scope(id);
        const auto connect_field = [&](const auto& prop, auto member) {
            connect_prop(scope, prop, [&services, id, row_id, member](const auto& value) {
                auto* col = services.components().state<ColState>(id);
                auto candidate = col->base;
                candidate.*member = value;
                validate(candidate);
                col->base = candidate;
                refresh(services, row_id);
            });
        };
        if (props.span_) {
            connect_field(*props.span_, &ColOptions::span);
        }
        connect_field(props.offset_, &ColOptions::offset);
        connect_field(props.order_, &ColOptions::order);
        connect_field(props.push_, &ColOptions::push);
        connect_field(props.pull_, &ColOptions::pull);
        if (props.flex_) {
            connect_field(*props.flex_, &ColOptions::flex);
        }
        connect_prop(scope, props.responsive_, [&services, id, row_id](const auto& value) {
            validate_responsive(value, [](const auto& options) { validate(options); });
            services.components().state<ColState>(id)->responsive = value;
            refresh(services, row_id);
        });
        build.mount_slot(id, content);
        refresh(services, row_id);
    }
};

std::vector<GridColumnSnapshot> grid_columns(const runtime::ComponentHost& host, runtime::ComponentId row) {
    const auto* state = host.state<RowState>(row);
    if (!state) {
        throw std::out_of_range("Grid Row is stale");
    }
    std::vector<GridColumnSnapshot> result;
    for (const auto id : state->columns) {
        if (const auto* col = host.state<ColState>(id)) {
            result.push_back({id, col->resolved, col->frame, col->resolved.span == 0});
        }
    }
    return result;
}

} // namespace ryn::detail

namespace ryn {

void Row(RowProps props, RowContent content) {
    detail::ConfigDefaults::apply(props);
    detail::GridPropsAccess::mount_row(props, content);
}

void Col(ColProps props, ColContent content) {
    detail::GridPropsAccess::mount_col(props, content);
}

} // namespace ryn
