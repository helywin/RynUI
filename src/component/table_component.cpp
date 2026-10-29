#include "component/table_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "reactive/untracked.hpp"
#include <ryn/button.hpp>
#include <ryn/checkbox.hpp>
#include <ryn/flex.hpp>
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <set>

namespace ryn::detail {
namespace {
thread_local TableComponentHost* active_tables{};
constexpr auto infinity = std::numeric_limits<float>::infinity();

struct CellState final {
    runtime::NodeId node;
    runtime::ComponentId content;
    runtime::Size measured;
    std::size_t column{};
};

struct RowState final {
    explicit RowState(TableRecord value) : record(std::move(value)), value(record.value) {}

    TableRecord record;
    Signal<TableValue> value;
    runtime::NodeId node;
    std::vector<runtime::ComponentId> cells;
    runtime::ComponentId checkbox;
    component::RetainedSurfaceId surface;
    input::InteractionId interaction;
    Signal<bool> checked{false};
    Signal<bool> disabled{false};
    float height{};
    bool header{};
    bool shown{};
};

struct TableState final {
    runtime::NodeId node;
    runtime::ComponentId header_viewport;
    runtime::ComponentId header;
    runtime::ComponentId body;
    runtime::ComponentId empty;
    runtime::ComponentId loading_label;
    component::RetainedSurfaceId loading_surface;
    runtime::ComponentId select_all;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId border_surface;
    TableOptions options;
    std::vector<TableColumnSchema> columns;
    std::vector<float> requested_widths;
    std::vector<float> widths;
    std::vector<TableAlign> alignments;
    std::vector<runtime::ComponentId> sort_buttons;
    std::vector<Signal<IconName>> sort_icons;
    std::map<std::string, runtime::ComponentId> by_key;
    std::vector<runtime::ComponentId> source_order;
    std::vector<TableRecord> records;
    std::optional<std::vector<TableRecord>> pending_records;
    std::vector<String> sorted_keys;
    std::vector<runtime::ComponentId> sorted_order;
    std::vector<runtime::ComponentId> visible;
    std::vector<String> selected;
    std::set<std::string, std::less<>> selected_keys;
    std::uint64_t projection_revision{1};
    std::uint64_t sorted_revision{};
    TableWork work;
    bool data_dirty{};
    bool page_dirty{true};
    bool order_dirty{true};
    bool refreshing{};
    bool pending_refresh{};
    bool pending_geometry{};
    bool ready{};
    TableSort sort;
    Signal<bool> blocked{false};
    Signal<bool> all_checked{false};
    Signal<bool> all_partial{false};
    Signal<bool> all_disabled{true};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> header_foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> header_typography{runtime::SemanticTypography{}};
    std::array<theme_runtime::Subscription, 3> subscriptions;
    std::optional<runtime::ComponentId> hovered;
    std::optional<input::InteractionId> last_focus;
    std::size_t current{1};
    std::size_t effective_current{1};
    std::size_t page_size{10};
    float viewport_height{240};
    float content_width{};
    float content_height{};
    float header_height{};
    float scroll_x{};
    float scroll_y{};
    float maximum_x{};
    float maximum_y{};
    float padding_x{16};
    float padding_y{16};
    runtime::Rect body_bounds;
    runtime::Rect clip;
    ControlSize size{ControlSize::Large};
    bool bordered{};
    bool loading{};
    bool show_header{true};
};

using TableStatePtr = std::shared_ptr<TableState>;

TableStatePtr table_state(WindowComponentServices& services, runtime::ComponentId id) {
    const auto* stored = services.components().state<TableStatePtr>(id);
    return stored ? *stored : TableStatePtr{};
}

bool live(WindowComponentServices& services, runtime::ComponentId id, const TableStatePtr& s) {
    return table_state(services, id) == s && services.components().scope(id).active();
}

struct ProjectionInterrupted final {};

void finite_nonnegative(float value, const char* message) {
    if (!std::isfinite(value) || value < 0) {
        throw std::invalid_argument(message);
    }
}

void positive_size(std::size_t value) {
    if (!value) {
        throw std::invalid_argument("Table page/current must be positive");
    }
}

void valid_align(TableAlign value) {
    if (value != TableAlign::Left && value != TableAlign::Center && value != TableAlign::Right) {
        throw std::invalid_argument("Table alignment is invalid");
    }
}

void valid_size(ControlSize value) {
    if (value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large) {
        throw std::invalid_argument("Table size is invalid");
    }
}

void validate_records(const std::vector<TableRecord>& records) {
    std::set<std::string> keys;
    for (const auto& record : records) {
        if (record.key.empty() || !record.value || !keys.insert(std::string{record.key.bytes()}).second) {
            throw std::invalid_argument("Table rowKey must be nonempty and unique");
        }
    }
}

void validate_sort(const TableSort& sort, const std::vector<TableColumnSchema>& columns) {
    if (sort.order != TableSortOrder::None && sort.order != TableSortOrder::Ascend &&
        sort.order != TableSortOrder::Descend) {
        throw std::invalid_argument("Table sort order is invalid");
    }
    if (sort.order != TableSortOrder::None && std::ranges::none_of(columns, [&](const auto& column) {
            return column.key == sort.column_key && column.sortable;
        })) {
        throw std::invalid_argument("Table sort column must be sortable");
    }
}

template <class Current>
std::vector<String> sort_records(const TableState& state, const std::vector<TableRecord>& records, Current current) {
    std::vector<const TableRecord*> order;
    order.reserve(records.size());
    for (const auto& record : records) {
        order.push_back(&record);
    }
    const auto sort = state.sort;
    if (sort.order != TableSortOrder::None) {
        const auto column =
            std::ranges::find_if(state.columns, [&](const auto& value) { return value.key == sort.column_key; });
        if (column != state.columns.end() && column->less) {
            // User code may replace data/sort, destroy this owner, or reuse its slot.
            const auto less = column->less;
            std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
                if (!current()) {
                    throw ProjectionInterrupted{};
                }
                const bool result =
                    sort.order == TableSortOrder::Descend ? less(b->value, a->value) : less(a->value, b->value);
                if (!current()) {
                    throw ProjectionInterrupted{};
                }
                return result;
            });
        }
    }
    std::vector<String> result;
    result.reserve(order.size());
    for (const auto* record : order) {
        result.push_back(record->key);
    }
    return result;
}

std::pair<std::size_t, std::size_t> page_range(TableState& state, std::size_t count) {
    std::size_t start = 0;
    if (state.options.current) {
        const auto pages = std::max(std::size_t{1}, count / state.page_size + (count % state.page_size != 0));
        state.effective_current = std::clamp(state.current, std::size_t{1}, pages);
        start = (state.effective_current - 1) * state.page_size;
        count = std::min(state.page_size, count - start);
    }
    return {start, count};
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

runtime::Rect translated(runtime::Rect rect, runtime::Point offset) {
    rect.x += offset.x;
    rect.y += offset.y;
    return rect;
}

bool contains(runtime::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

void translate(runtime::NodeId node, runtime::Point offset, WindowComponentServices& services) {
    if (services.nodes().require(node).clip_boundary) {
        return;
    }
    runtime::NodePropertyWriter{services.nodes(), services.dirty()}.set_translation(node, offset);
    for (auto child : services.nodes().require(node).children) {
        translate(child, offset, services);
    }
}

runtime::ComponentId region(runtime::ComponentId parent, WindowComponentServices& services, const Content& content,
                            const Signal<runtime::SemanticForeground>& color,
                            const Signal<runtime::SemanticTypography>& font, bool shown = true) {
    auto& build = runtime::require_component_build_context();
    runtime::ComponentId result;
    build.mount_slot(
        parent, Content{[&] {
            auto& context = runtime::require_component_build_context();
            result = context.mount_component<int>(0);
            services.components().set_branch_active(result, shown);
            const auto node = context.root(result);
            services.layout().set_layout(node, layout::BoxLayout{});
            context.on_resource_cleanup(result, [&services, node] { services.layout().remove_layout(node); });
            context.mount_slot_with_semantic_text_style(result, content, Prop<runtime::SemanticForeground>{color},
                                                        Prop<runtime::SemanticTypography>{font});
        }});
    return result;
}

void active(WindowComponentServices& services, runtime::ComponentId id, bool value) {
    if (services.components().set_branch_active(id, value)) {
        services.mark_scene_structure_dirty();
    }
}
} // namespace

TableComponentHost::TableComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

TableComponentHost::~TableComponentHost() {
    services_->detach(*this);
}

void* TableComponentHost::begin_mount() noexcept {
    auto* previous = active_tables;
    active_tables = this;
    return previous;
}

void TableComponentHost::end_mount(void* previous) noexcept {
    active_tables = static_cast<TableComponentHost*>(previous);
}

void TableComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

TableSnapshot TableComponentHost::snapshot(runtime::ComponentId id) const {
    const auto s = table_state(*services_, id);
    if (!s) {
        throw std::out_of_range("Table is stale");
    }
    TableSnapshot result;
    for (auto row_id : s->source_order) {
        const auto& row = *services_->components().state<RowState>(row_id);
        result.rows.push_back({row.record.key, row_id, row.cells, row.checkbox});
    }
    for (auto row_id : s->visible) {
        result.visible_keys.push_back(services_->components().state<RowState>(row_id)->record.key);
    }
    result.sort_buttons = s->sort_buttons;
    result.select_all = s->select_all;
    result.body = graphics::intersect_effect_bounds(
        translated(s->body_bounds, services_->nodes().require(s->node).translation), s->clip);
    result.widths = s->widths;
    result.scroll_x = s->scroll_x;
    result.scroll_y = s->scroll_y;
    result.maximum_x = s->maximum_x;
    result.maximum_y = s->maximum_y;
    result.current = s->effective_current;
    result.sort = s->sort;
    result.loading = s->loading;
    return result;
}

TableWork TableComponentHost::work(runtime::ComponentId id) const {
    const auto s = table_state(*services_, id);
    return s ? s->work : TableWork{};
}

runtime::ComponentId TableComponentHost::mount_row(runtime::ComponentId id, const TableRecord* record, bool header,
                                                   bool shown) {
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto owner = table_state(services, id);
    auto& s = *owner;
    const auto row_id = build.mount_component<RowState>(record ? *record : TableRecord{});
    const auto intact = [&] {
        if (!live(services, id, owner)) {
            return false;
        }
        if (!services.components().contains(row_id)) {
            services.retire_failed_component(id);
            return false;
        }
        return true;
    };
    auto& row = build.state<RowState>(row_id);
    row.node = build.root(row_id);
    row.header = header;
    if (!shown) {
        services.components().set_branch_active(row_id, false);
    }
    row.interaction = services.interactions().create({row_id, row.node, {}, true, false, {}, true, false});
    const auto hit_fragment = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit_fragment, {}, row.interaction);
    const auto fragment = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
    row.surface = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(row_id,
                              [&services, node = row.node, surface = row.surface, hit = row.interaction, hit_fragment] {
                                  services.pointer().cancel_interaction(hit);
                                  services.interactions().remove(hit);
                                  services.scene_composer().remove_fragment(hit_fragment);
                                  services.surfaces().destroy_content_range(surface);
                                  services.layout().remove_layout(node);
                              });
    build.mount_slot(
        row_id, Content{[&] {
            auto& cells = runtime::require_component_build_context();
            for (std::size_t index = 0; index < s.widths.size(); ++index) {
                const auto cell_id = cells.mount_component<CellState>();
                auto& cell = cells.state<CellState>(cell_id);
                cell.node = cells.root(cell_id);
                cell.column = index;
                services.nodes().require(cell.node).clip_content = true;
                cells.on_resource_cleanup(cell_id,
                                          [&services, node = cell.node] { services.layout().remove_layout(node); });
                const bool selection = s.options.selected && index == 0;
                const auto column_index = index - (s.options.selected ? 1 : 0);
                const auto content = Content{[&, selection, column_index] {
                    if (selection) {
                        CheckboxProps props;
                        props.checked(header ? s.all_checked : row.checked)
                            .disabled(header ? s.all_disabled : row.disabled)
                            .skipGroup(true)
                            .wave(false)
                            .onChange(
                                [this, id, key = header ? std::optional<String>{} : std::optional{row.record.key}](
                                    bool checked) { request_selection(id, key, checked); });
                        if (header) {
                            props.indeterminate(s.all_partial);
                        }
                        Checkbox(std::move(props));
                        if (!intact()) {
                            return;
                        }
                        const auto hit = services.interactions().declaration_order().back();
                        row.checkbox = services.interactions().require(hit).component;
                        if (header) {
                            s.select_all = row.checkbox;
                        }
                    } else if (header) {
                        Flex(FlexProps{}.align(FlexAlign::Center).gap(dp(4)), FlexContent{[&, column_index] {
                                 const auto column = s.columns[column_index];
                                 if (column.header) {
                                     SlotContentAccess::function (*column.header)();
                                 } else {
                                     Text(TextProps{}.content(column.title));
                                 }
                                 if (!intact()) {
                                     return;
                                 }
                                 if (column.sortable) {
                                     Button(ButtonProps{}
                                                .type(ButtonType::Text)
                                                .size(ControlSize::Small)
                                                .wave(false)
                                                .disabled(s.blocked)
                                                .onClick([this, id, column_index] { request_sort(id, column_index); }),
                                            ButtonContent{
                                                [icon = s.sort_icons[column_index]] { Icon(IconProps{}.name(icon)); }});
                                     if (!intact()) {
                                         return;
                                     }
                                     const auto hit = services.interactions().declaration_order().back();
                                     s.sort_buttons[column_index] = services.interactions().require(hit).component;
                                 }
                             }});
                    } else {
                        s.columns[column_index].cell(row.record.key, bind([value = row.value] { return value.get(); }));
                    }
                }};
                const auto content_id = region(cell_id, services, content, header ? s.header_foreground : s.foreground,
                                               header ? s.header_typography : s.typography);
                if (!intact()) {
                    return;
                }
                if (!services.components().contains(cell_id) || !services.components().contains(content_id)) {
                    services.retire_failed_component(id);
                    return;
                }
                cell.content = content_id;
                services.layout().set_layout(
                    cell.node,
                    layout::ComponentLayout{
                        [this, id, cell_id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                            auto& s = *table_state(*services_, id);
                            auto& cell = *services_->components().state<CellState>(cell_id);
                            const auto padding = s.options.selected && cell.column == 0
                                                     ? 0.0F
                                                     : std::min(s.padding_x, limits.max_width / 2);
                            cell.measured =
                                engine.measure_child(services_->components().root(cell.content),
                                                     {0, std::max(0.0F, limits.max_width - padding * 2), 0, infinity});
                            return limits.constrain({limits.max_width, cell.measured.height + s.padding_y * 2});
                        },
                        [this, id, cell_id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                            auto& s = *table_state(*services_, id);
                            auto& cell = *services_->components().state<CellState>(cell_id);
                            const auto padding =
                                s.options.selected && cell.column == 0 ? 0.0F : std::min(s.padding_x, bounds.width / 2);
                            const auto width =
                                std::min(cell.measured.width, std::max(0.0F, bounds.width - padding * 2));
                            const auto align = s.alignments[cell.column];
                            const auto x = align == TableAlign::Right    ? bounds.x + bounds.width - padding - width
                                           : align == TableAlign::Center ? bounds.x + (bounds.width - width) / 2
                                                                         : bounds.x + padding;
                            engine.place_child(services_->components().root(cell.content),
                                               {x, bounds.y + (bounds.height - cell.measured.height) / 2, width,
                                                cell.measured.height});
                        }});
                row.cells.push_back(cell_id);
            }
        }});
    if (!intact()) {
        return {};
    }
    services.layout().set_layout(
        row.node, layout::ComponentLayout{
                      [this, id, row_id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                          auto& s = *table_state(*services_, id);
                          auto& row = *services_->components().state<RowState>(row_id);
                          row.height = 0;
                          for (std::size_t i = 0; i < row.cells.size(); ++i) {
                              const auto size = engine.measure_child(services_->components().root(row.cells[i]),
                                                                     {s.widths[i], s.widths[i], 0, infinity});
                              row.height = std::max(row.height, size.height);
                          }
                          const auto& token = services_->components().theme_scope(id)->snapshot().table();
                          row.height = std::max(row.height, token.metrics.line_height + s.padding_y * 2);
                          return limits.constrain({s.content_width, row.height});
                      },
                      [this, id, row_id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                          auto& s = *table_state(*services_, id);
                          auto& row = *services_->components().state<RowState>(row_id);
                          float x = bounds.x;
                          for (std::size_t i = 0; i < row.cells.size(); ++i) {
                              engine.place_child(services_->components().root(row.cells[i]),
                                                 {x, bounds.y, s.widths[i], bounds.height}, false, true);
                              x += s.widths[i];
                          }
                      }});
    return row_id;
}

void TableComponentHost::reconcile(runtime::ComponentId id, std::vector<TableRecord> records) {
    validate_records(records);
    const auto s = table_state(*services_, id);
    if (!s) {
        return;
    }
    s->pending_records = std::move(records);
    ++s->projection_revision;
    refresh(id, true);
}

void TableComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    const auto s = table_state(*services_, id);
    if (!s || !live(*services_, id, s)) {
        return;
    }
    s->pending_refresh = true;
    s->pending_geometry |= geometry;
    if (!s->ready || s->refreshing) {
        return;
    }
    s->refreshing = true;

    struct Reset final {
        TableStatePtr s;
        bool& geometry;
        int exceptions{std::uncaught_exceptions()};

        ~Reset() {
            if (std::uncaught_exceptions() > exceptions) {
                s->pending_refresh = true;
                s->pending_geometry |= geometry;
            }
            s->refreshing = false;
        }
    } reset{s, geometry};

    const auto alive = [&] {
        return live(*services_, id, s);
    };
    while (s->pending_refresh && alive()) {
        s->pending_refresh = false;
        geometry = std::exchange(s->pending_geometry, false);
        if (s->pending_records) {
            s->records = std::move(*s->pending_records);
            s->pending_records.reset();
            s->data_dirty = true;
        }
        const auto revision = s->projection_revision;
        const auto current = [&] {
            return alive() && s->projection_revision == revision;
        };
        const bool sorted_changed = s->sorted_revision != revision;
        if (sorted_changed) {
            // This immutable copy owns all values referenced by the user comparator.
            // Pure selection/border/page refreshes never enter this branch.
            const auto records = s->records;
            s->work.copied_records += records.size();
            ++s->work.sort_passes;
            try {
                std::vector<String> sorted;
                run_untracked([&] { sorted = sort_records(*s, records, current); });
                if (!current()) {
                    continue;
                }
                s->sorted_keys = std::move(sorted);
                s->sorted_revision = revision;
                s->order_dirty = true;
                s->page_dirty = true;
            } catch (const ProjectionInterrupted&) {
                continue;
            }
        }
        if (s->data_dirty) {
            std::set<std::string, std::less<>> next_keys;
            for (const auto& record : s->records) {
                next_keys.emplace(record.key.bytes());
            }
            std::vector<runtime::ComponentId> removed;
            for (auto iterator = s->by_key.begin(); iterator != s->by_key.end();) {
                if (!next_keys.contains(iterator->first)) {
                    removed.push_back(iterator->second);
                    std::erase(s->source_order, iterator->second);
                    std::erase(s->visible, iterator->second);
                    iterator = s->by_key.erase(iterator);
                } else {
                    ++iterator;
                }
            }
            for (auto row : removed) {
                services_->destroy(row);
                if (!alive()) {
                    return;
                }
            }
            if (!current()) {
                continue;
            }
            const auto [start, count] = page_range(*s, s->sorted_keys.size());
            std::set<std::string_view> shown_keys;
            for (std::size_t i = start; i < start + count; ++i) {
                shown_keys.insert(s->sorted_keys[i].bytes());
            }
            std::vector<runtime::ComponentId> source_order;
            source_order.reserve(s->records.size());
            for (const auto& record : s->records) {
                const auto key = std::string{record.key.bytes()};
                auto found = s->by_key.find(key);
                if (found == s->by_key.end()) {
                    runtime::ComponentId row;
                    services_->append_slot(
                        s->body,
                        Content{[&] { row = mount_row(id, &record, false, shown_keys.contains(record.key.bytes())); }});
                    if (!alive()) {
                        return;
                    }
                    if (!services_->components().contains(row)) {
                        services_->retire_failed_component(id);
                        return;
                    }
                    found = s->by_key.emplace(key, row).first;
                } else {
                    auto* row = services_->components().state<RowState>(found->second);
                    if (!row) {
                        services_->retire_failed_component(id);
                        return;
                    }
                    row->record = record;
                    // Keep the signal alive if a cell binding destroys its own row/owner.
                    const auto value = row->value;
                    value.set(record.value);
                }
                if (!current()) {
                    break;
                }
                source_order.push_back(found->second);
            }
            if (!current()) {
                continue;
            }
            s->source_order = std::move(source_order);
            s->data_dirty = false;
            s->order_dirty = true;
            geometry = true;
        }
        if (sorted_changed || s->order_dirty) {
            std::vector<runtime::ComponentId> sorted_order;
            sorted_order.reserve(s->sorted_keys.size());
            for (const auto& key : s->sorted_keys) {
                sorted_order.push_back(s->by_key.at(std::string{key.bytes()}));
            }
            s->sorted_order = std::move(sorted_order);
            s->page_dirty = true;
        }
        if (s->page_dirty) {
            const auto [start, count] = page_range(*s, s->sorted_order.size());
            std::vector<runtime::ComponentId> visible(s->sorted_order.begin() + start,
                                                      s->sorted_order.begin() + start + count);
            s->visible = std::move(visible);
            for (auto row_id : s->source_order) {
                auto* row = services_->components().state<RowState>(row_id);
                if (!row) {
                    services_->retire_failed_component(id);
                    return;
                }
                row->shown = false;
            }
            for (auto row_id : s->visible) {
                services_->components().state<RowState>(row_id)->shown = true;
            }
            s->page_dirty = false;
        }
        const auto& theme = services_->components().theme_scope(id)->snapshot();
        const auto token = theme.table();
        const auto text = theme.text();
        if (s->options.selected) {
            s->requested_widths[0] = token.metrics.selection_width;
        }
        s->foreground.set(foreground(token.colors.text));
        if (!alive()) {
            return;
        }
        s->header_foreground.set(foreground(token.colors.header_text));
        if (!alive()) {
            return;
        }
        s->typography.set(
            {text.font_family, text.font_weight, false, token.metrics.font_size, token.metrics.line_height});
        if (!alive()) {
            return;
        }
        s->header_typography.set({text.font_family, 600, false, token.metrics.font_size, token.metrics.line_height});
        if (!alive()) {
            return;
        }
        s->padding_y = s->size == ControlSize::Small    ? token.metrics.padding_block_small
                       : s->size == ControlSize::Middle ? token.metrics.padding_block_middle
                                                        : token.metrics.padding_block;
        s->padding_x =
            s->size == ControlSize::Large ? token.metrics.padding_inline : token.metrics.padding_inline_small;
        s->blocked.set(s->loading);
        if (!alive()) {
            return;
        }
        if (s->order_dirty) {
            // Keep every retained row's interaction block in display order, including
            // newly appended controls that were declared after external siblings.
            const auto* header = services_->components().state<RowState>(s->header);
            if (!header) {
                services_->retire_failed_component(id);
                return;
            }
            auto anchor = header->interaction;
            std::map<std::uint32_t, std::pair<runtime::ComponentId, std::vector<input::InteractionId>>>
                row_interactions;
            for (auto row : s->source_order) {
                row_interactions.try_emplace(row.index, row, std::vector<input::InteractionId>{});
            }
            for (auto interaction : services_->interactions().declaration_order()) {
                for (auto ancestor = std::optional{services_->interactions().require(interaction).component};
                     ancestor && *ancestor != id; ancestor = services_->components().parent(*ancestor)) {
                    if (*ancestor == s->header) {
                        anchor = interaction;
                        break;
                    }
                    if (auto found = row_interactions.find(ancestor->index);
                        found != row_interactions.end() && found->second.first == *ancestor) {
                        found->second.second.push_back(interaction);
                        break;
                    }
                }
            }
            std::vector<input::InteractionId> order;
            // Off-page rows are already excluded by branch eligibility. Keeping
            // the full sorted order makes page changes independent of this work.
            for (auto row : s->sorted_order) {
                const auto& interactions = row_interactions.at(row.index).second;
                order.insert(order.end(), interactions.begin(), interactions.end());
            }
            services_->interactions().reorder_after(anchor, order);
            s->order_dirty = false;
        }
        std::size_t selectable = 0;
        std::size_t checked = 0;
        for (auto row_id : s->source_order) {
            auto* row = services_->components().state<RowState>(row_id);
            if (!row) {
                services_->retire_failed_component(id);
                return;
            }
            ++s->work.visited_rows;
            const bool shown = row->shown;
            active(*services_, row_id, shown);
            const bool selected = s->selected_keys.contains(row->record.key.bytes());
            const bool disabled = row->record.selection_disabled;
            const auto row_checked = row->checked;
            const auto row_disabled = row->disabled;
            row_checked.set(selected);
            if (!alive()) {
                return;
            }
            row_disabled.set(s->loading || disabled);
            if (!alive()) {
                return;
            }
            if (!services_->components().contains(row_id)) {
                services_->retire_failed_component(id);
                return;
            }
            if (shown && !disabled) {
                ++selectable;
                if (selected) {
                    ++checked;
                }
            }
        }
        s->all_checked.set(selectable > 0 && checked == selectable);
        if (!alive()) {
            return;
        }
        s->all_partial.set(checked > 0 && checked < selectable);
        if (!alive()) {
            return;
        }
        s->all_disabled.set(s->loading || selectable == 0);
        if (!alive()) {
            return;
        }
        active(*services_, s->header_viewport, s->show_header);
        active(*services_, s->empty, s->visible.empty() && !s->loading);
        active(*services_, s->loading_label, s->loading);
        for (std::size_t i = 0; i < s->columns.size(); ++i) {
            s->sort_icons[i].set(s->sort.column_key != s->columns[i].key || s->sort.order == TableSortOrder::None
                                     ? IconName::ColumnHeightOutlined
                                 : s->sort.order == TableSortOrder::Ascend ? IconName::ArrowUpOutlined
                                                                           : IconName::ArrowDownOutlined);
            if (!alive()) {
                return;
            }
        }
        services_->dirty().invalidate(s->node, geometry
                                                   ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                         runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                   : runtime::DirtyFlags::Material);
        if (geometry) {
            const auto flags = runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout;
            services_->dirty().invalidate(services_->components().root(s->body), flags);
            services_->dirty().invalidate(services_->components().root(s->header_viewport), flags);
            auto rows = s->source_order;
            rows.push_back(s->header);
            for (auto row_id : rows) {
                const auto& row = *services_->components().state<RowState>(row_id);
                services_->dirty().invalidate(row.node, flags);
                for (auto cell : row.cells) {
                    services_->dirty().invalidate(services_->components().root(cell), flags);
                }
            }
        }
    }
}

void TableComponentHost::request_sort(runtime::ComponentId id, std::size_t column) {
    auto s = table_state(*services_, id);
    if (!s || s->loading || !s->columns[column].sortable) {
        return;
    }
    TableSort value{s->columns[column].key, TableSortOrder::Ascend};
    if (s->sort.column_key == value.column_key) {
        value.order = s->sort.order == TableSortOrder::Ascend    ? TableSortOrder::Descend
                      : s->sort.order == TableSortOrder::Descend ? TableSortOrder::None
                                                                 : TableSortOrder::Ascend;
    }
    const auto callback = s->options.on_sort;
    if (callback) {
        callback(std::move(value));
    }
}

void TableComponentHost::request_selection(runtime::ComponentId id, std::optional<String> key, bool selected) {
    auto s = table_state(*services_, id);
    if (!s || s->loading || !s->options.selected) {
        return;
    }
    auto value = s->selected;
    for (auto row_id : s->visible) {
        const auto& row = *services_->components().state<RowState>(row_id);
        if (row.record.selection_disabled || (key && row.record.key != *key)) {
            continue;
        }
        const auto found = std::ranges::find(value, row.record.key);
        if (selected && found == value.end()) {
            value.push_back(row.record.key);
        } else if (!selected && found != value.end()) {
            value.erase(found);
        }
    }
    const auto callback = s->options.on_selection;
    if (callback && value != s->selected) {
        callback(std::move(value));
    }
}

void TableComponentHost::mount(TableOptions options, const Prop<std::vector<TableRecord>>& source,
                               std::vector<TableColumnSchema> columns, const std::optional<TableEmpty>& empty) {
    const auto records = read_prop(source);
    validate_records(records);
    if (columns.empty()) {
        throw std::invalid_argument("Table requires at least one column");
    }
    std::set<std::string> keys;
    for (const auto& column : columns) {
        if (column.key.empty() || !column.cell || !keys.insert(std::string{column.key.bytes()}).second) {
            throw std::invalid_argument("Table column key must be nonempty and unique");
        }
        finite_nonnegative(read_prop(column.width), "Table column width must be finite and nonnegative");
        valid_align(read_prop(column.align));
    }
    positive_size(read_prop(options.page_size));
    if (options.current) {
        positive_size(read_prop(*options.current));
    }
    finite_nonnegative(read_prop(options.viewport_height), "Table scrollY must be finite and nonnegative");
    valid_size(read_prop(options.size));
    validate_sort(read_prop(options.sort), columns);
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto owner = std::make_shared<TableState>();
    const auto id = build.mount_component<TableStatePtr>(owner);
    auto& s = *owner;
    s.node = build.root(id);
    s.columns = std::move(columns);
    s.options = std::move(options);
    s.size = read_prop(s.options.size);
    s.loading = read_prop(s.options.loading);
    s.bordered = read_prop(s.options.bordered);
    s.show_header = read_prop(s.options.show_header);
    s.viewport_height = read_prop(s.options.viewport_height);
    s.current = s.options.current ? read_prop(*s.options.current) : 1;
    s.page_size = read_prop(s.options.page_size);
    s.sort = read_prop(s.options.sort);
    if (s.options.selected) {
        s.selected = read_prop(*s.options.selected);
        for (const auto& key : s.selected) {
            s.selected_keys.emplace(key.bytes());
        }
        s.requested_widths.push_back(32);
        s.alignments.push_back(TableAlign::Center);
    }
    for (const auto& column : s.columns) {
        s.requested_widths.push_back(read_prop(column.width));
        s.alignments.push_back(read_prop(column.align));
        s.sort_icons.emplace_back(IconName::ColumnHeightOutlined);
    }
    s.widths = s.requested_widths;
    s.sort_buttons.resize(s.columns.size());
    s.records = records;
    try {
        s.sorted_keys = sort_records(s, records, [&] { return live(services, id, owner); });
    } catch (const ProjectionInterrupted&) {
        return;
    }
    s.sorted_revision = s.projection_revision;
    const auto [page_start, page_count] = page_range(s, records.size());
    std::set<std::string_view> shown_keys;
    for (std::size_t i = page_start; i < page_start + page_count; ++i) {
        shown_keys.insert(s.sorted_keys[i].bytes());
    }
    runtime::connect_layout_style(build.scope(id), s.options.layout, s.node, services.nodes(), services.dirty());
    if (!live(services, id, owner)) {
        return;
    }
    services.nodes().require(s.node).clip_content = true;
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    s.surface = services.surfaces().create_content_range(fragment, {});
    const auto border_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.border_surface = services.surfaces().create_content_range(border_fragment, {});
    build.on_resource_cleanup(
        id, [&services, surface = s.border_surface] { services.surfaces().destroy_content_range(surface); });
    build.on_resource_cleanup(id, [&services, node = s.node, surface = s.surface] {
        services.surfaces().destroy_content_range(surface);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            s.header_viewport = context.mount_component<int>(0);
            const auto header_node = context.root(s.header_viewport);
            services.components().set_branch_active(s.header_viewport, s.show_header);
            services.nodes().require(header_node).clip_content = true;
            context.on_resource_cleanup(s.header_viewport,
                                        [&services, header_node] { services.layout().remove_layout(header_node); });
            context.mount_slot(s.header_viewport, Content{[&] { s.header = mount_row(id, nullptr, true); }});
            if (!live(services, id, owner)) {
                return;
            }
            services.layout().set_layout(
                header_node, layout::ComponentLayout{
                                 [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                                     auto& s = *table_state(*services_, id);
                                     const auto size =
                                         engine.measure_child(services_->components().root(s.header),
                                                              {s.content_width, s.content_width, 0, infinity});
                                     return limits.constrain({limits.max_width, size.height});
                                 },
                                 [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                                     auto& s = *table_state(*services_, id);
                                     engine.place_child(services_->components().root(s.header),
                                                        {bounds.x, bounds.y, s.content_width, bounds.height});
                                 }});
            s.body = context.mount_component<int>(0);
            const auto body_node = context.root(s.body);
            services.nodes().require(body_node).clip_content = true;
            const auto body_hit = services.interactions().create({s.body, body_node, {}, true, false, {}, true, false});
            const auto body_fragment =
                context.register_scene_fragment(s.body, runtime::SceneFragmentPlacement::before_children);
            services.scene_composer().set_fragment(body_fragment, {}, body_hit);
            context.on_resource_cleanup(s.body, [&services, body_node, body_hit, body_fragment] {
                services.pointer().cancel_interaction(body_hit);
                services.interactions().remove(body_hit);
                services.scene_composer().remove_fragment(body_fragment);
                services.layout().remove_layout(body_node);
            });
            context.mount_slot(s.body, Content{[&] {
                                   for (const auto& record : records) {
                                       const auto row =
                                           mount_row(id, &record, false, shown_keys.contains(record.key.bytes()));
                                       if (!live(services, id, owner)) {
                                           return;
                                       }
                                       s.by_key.emplace(std::string{record.key.bytes()}, row);
                                       s.source_order.push_back(row);
                                   }
                                   s.empty = region(s.body, services, Content{[empty] {
                                                        if (empty) {
                                                            SlotContentAccess::function (*empty)();
                                                        } else {
                                                            Text(u8"No data");
                                                        }
                                                    }},
                                                    s.foreground, s.typography, shown_keys.empty() && !s.loading);
                               }});
            if (!live(services, id, owner)) {
                return;
            }
            s.loading_label =
                region(id, services, Content{[] { Text(u8"Loading…"); }}, s.foreground, s.typography, s.loading);
            const auto loading_fragment =
                context.register_scene_fragment(s.loading_label, runtime::SceneFragmentPlacement::before_children);
            s.loading_surface = services.surfaces().create_content_range(loading_fragment, {});
            context.on_resource_cleanup(s.loading_label, [&services, surface = s.loading_surface] {
                services.surfaces().destroy_content_range(surface);
            });
            services.layout().set_layout(
                body_node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& s = *table_state(*services_, id);
                        s.content_height = 0;
                        for (auto row : s.visible) {
                            s.content_height += engine
                                                    .measure_child(services_->components().root(row),
                                                                   {s.content_width, s.content_width, 0, infinity})
                                                    .height;
                        }
                        if (s.visible.empty()) {
                            static_cast<void>(engine.measure_child(services_->components().root(s.empty),
                                                                   {0, limits.max_width, 0, infinity}));
                        }
                        return limits.constrain(
                            {limits.max_width,
                             std::min(
                                 s.viewport_height,
                                 s.visible.empty()
                                     ? services_->components().theme_scope(id)->snapshot().table().metrics.empty_height
                                     : s.content_height)});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        auto& s = *table_state(*services_, id);
                        s.body_bounds = bounds;
                        float y = bounds.y;
                        for (auto row_id : s.visible) {
                            const auto& row = *services_->components().state<RowState>(row_id);
                            engine.place_child(row.node, {bounds.x, y, s.content_width, row.height});
                            y += row.height;
                        }
                        if (s.visible.empty()) {
                            const auto size =
                                services_->nodes().require(services_->components().root(s.empty)).measured_size;
                            engine.place_child(services_->components().root(s.empty),
                                               {bounds.x + (bounds.width - size.width) / 2,
                                                bounds.y + (bounds.height - size.height) / 2, size.width, size.height});
                        }
                    }});
        }});
    if (!live(services, id, owner)) {
        return;
    }
    services.layout().set_layout(
        s.node,
        layout::ComponentLayout{
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                auto& s = *table_state(*services_, id);
                const auto& m = services_->components().theme_scope(id)->snapshot().table().metrics;
                float fixed = 0;
                std::size_t automatic = 0;
                for (auto width : s.requested_widths) {
                    if (width == 0) {
                        ++automatic;
                    } else {
                        fixed += width;
                    }
                }
                const float viewport_width = std::isfinite(limits.max_width)
                                                 ? limits.max_width
                                                 : fixed + static_cast<float>(automatic) * m.min_column_width;
                const float remaining = std::max(0.0F, viewport_width - fixed);
                s.content_width = 0;
                for (std::size_t i = 0; i < s.widths.size(); ++i) {
                    s.widths[i] =
                        s.requested_widths[i] > 0
                            ? s.requested_widths[i]
                            : std::max(m.min_column_width, automatic ? remaining / static_cast<float>(automatic) : 0);
                    s.content_width += s.widths[i];
                }
                if (!std::isfinite(s.content_width)) {
                    throw std::invalid_argument("Table total column width must be finite");
                }
                s.content_width = std::max(viewport_width, s.content_width);
                s.header_height = s.show_header
                                      ? engine
                                            .measure_child(services_->components().root(s.header_viewport),
                                                           {viewport_width, viewport_width, 0, limits.max_height})
                                            .height
                                      : 0;
                const auto body = engine.measure_child(
                    services_->components().root(s.body),
                    {viewport_width, viewport_width, 0, std::max(0.0F, limits.max_height - s.header_height)});
                static_cast<void>(engine.measure_child(services_->components().root(s.loading_label),
                                                       {0, viewport_width, 0, infinity}));
                return limits.constrain({viewport_width, s.header_height + body.height});
            },
            [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                auto& s = *table_state(*services_, id);
                if (s.show_header) {
                    engine.place_child(services_->components().root(s.header_viewport),
                                       {bounds.x, bounds.y, bounds.width, s.header_height});
                }
                const auto height = std::min(s.viewport_height, std::max(0.0F, bounds.height - s.header_height));
                engine.place_child(services_->components().root(s.body),
                                   {bounds.x, bounds.y + s.header_height, bounds.width, height});
                const auto loading_size =
                    services_->nodes().require(services_->components().root(s.loading_label)).measured_size;
                engine.place_child(services_->components().root(s.loading_label),
                                   {bounds.x + (bounds.width - loading_size.width) / 2,
                                    bounds.y + s.header_height + (height - loading_size.height) / 2, loading_size.width,
                                    loading_size.height});
                s.maximum_x = std::max(0.0F, s.content_width - bounds.width);
                s.maximum_y = std::max(0.0F, s.content_height - height);
                s.scroll_x = std::clamp(s.scroll_x, 0.0F, s.maximum_x);
                s.scroll_y = std::clamp(s.scroll_y, 0.0F, s.maximum_y);
            }});
    mounted_.push_back(id);
    s.ready = true;
    refresh(id, true);
    if (!live(services, id, owner)) {
        return;
    }
    auto& scope = build.scope(id);
    connect_prop(scope, source, [this, id](std::vector<TableRecord> value) {
        run_untracked([&] { reconcile(id, std::move(value)); });
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.sort, [this, id](TableSort value) {
        auto& s = *table_state(*services_, id);
        validate_sort(value, s.columns);
        if (s.sort != value) {
            s.scroll_y = 0;
            ++s.projection_revision;
        }
        s.sort = std::move(value);
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    if (s.options.selected) {
        connect_prop(scope, *s.options.selected, [this, id](std::vector<String> value) {
            auto& s = *table_state(*services_, id);
            std::set<std::string, std::less<>> seen;
            for (const auto& key : value) {
                if (key.empty() || !seen.insert(std::string{key.bytes()}).second) {
                    throw std::invalid_argument("Table selectedRowKeys must be nonempty and unique");
                }
            }
            s.selected = std::move(value);
            s.selected_keys = std::move(seen);
            refresh(id, false);
        });
    }
    if (!live(services, id, owner)) {
        return;
    }
    if (s.options.current) {
        connect_prop(scope, *s.options.current, [this, id](std::size_t value) {
            positive_size(value);
            auto& s = *table_state(*services_, id);
            s.current = value;
            s.page_dirty = true;
            s.scroll_y = 0;
            refresh(id, true);
        });
    }
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.page_size, [this, id](std::size_t value) {
        positive_size(value);
        auto& s = *table_state(*services_, id);
        s.page_size = value;
        s.page_dirty = true;
        s.scroll_y = 0;
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.viewport_height, [this, id](float value) {
        finite_nonnegative(value, "Table scrollY must be finite and nonnegative");
        table_state(*services_, id)->viewport_height = value;
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.size, [this, id](ControlSize value) {
        valid_size(value);
        table_state(*services_, id)->size = value;
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.loading, [this, id](bool value) {
        table_state(*services_, id)->loading = value;
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.bordered, [this, id](bool value) {
        table_state(*services_, id)->bordered = value;
        refresh(id, false);
    });
    if (!live(services, id, owner)) {
        return;
    }
    connect_prop(scope, s.options.show_header, [this, id](bool value) {
        table_state(*services_, id)->show_header = value;
        refresh(id, true);
    });
    if (!live(services, id, owner)) {
        return;
    }
    for (std::size_t i = 0; i < s.columns.size(); ++i) {
        const auto index = i + (s.options.selected ? 1 : 0);
        connect_prop(scope, s.columns[i].width, [this, id, index](float value) {
            finite_nonnegative(value, "Table width must be finite and nonnegative");
            table_state(*services_, id)->requested_widths[index] = value;
            refresh(id, true);
        });
        if (!live(services, id, owner)) {
            return;
        }
        connect_prop(scope, s.columns[i].align, [this, id, index](TableAlign value) {
            valid_align(value);
            table_state(*services_, id)->alignments[index] = value;
            refresh(id, true);
        });
        if (!live(services, id, owner)) {
            return;
        }
    }
    const auto theme = build.theme_scope();
    for (std::size_t i = 0; i < s.subscriptions.size(); ++i) {
        // Table metrics own typography dimensions; family and weight follow Text.
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i != 0](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, i] {
                               if (i == 0) {
                                   static_cast<void>(theme->table_colors());
                               } else if (i == 1) {
                                   static_cast<void>(theme->table_metrics());
                               } else {
                                   static_cast<void>(theme->text_font_family());
                                   static_cast<void>(theme->text_font_weight());
                               }
                           });
        if (!live(services, id, owner)) {
            return;
        }
    }
}

void TableComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto s = table_state(*services_, id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const auto focused = services_->focus().state().focused;
        if (s->last_focus != focused) {
            s->last_focus = focused;
            const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
            for (auto ancestor = target ? std::optional{target->component} : std::nullopt; ancestor;
                 ancestor = services_->components().parent(*ancestor)) {
                if (std::ranges::find(s->visible, *ancestor) == s->visible.end()) {
                    continue;
                }
                const auto& target_node = services_->nodes().require(target->node);
                const auto left = target_node.bounds.x - s->body_bounds.x;
                const auto top = target_node.bounds.y - s->body_bounds.y;
                const auto right = left + std::min(target_node.bounds.width, s->body_bounds.width);
                const auto bottom = top + std::min(target_node.bounds.height, s->body_bounds.height);
                if (left < s->scroll_x) {
                    s->scroll_x = std::max(0.0F, left);
                } else if (right > s->scroll_x + s->body_bounds.width) {
                    s->scroll_x = std::min(s->maximum_x, right - s->body_bounds.width);
                }
                if (top < s->scroll_y) {
                    s->scroll_y = std::max(0.0F, top);
                } else if (bottom > s->scroll_y + s->body_bounds.height) {
                    s->scroll_y = std::min(s->maximum_y, bottom - s->body_bounds.height);
                }
                break;
            }
        }
        const auto offset = services_->nodes().require(s->node).translation;
        translate(services_->components().root(s->header_viewport), offset, *services_);
        translate(services_->components().root(s->header), {offset.x - s->scroll_x, offset.y}, *services_);
        translate(services_->components().root(s->body), offset, *services_);
        for (auto row : s->visible) {
            translate(services_->components().root(row), {offset.x - s->scroll_x, offset.y - s->scroll_y}, *services_);
        }
        translate(services_->components().root(s->loading_label), offset, *services_);
    }
}

bool TableComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (mounted_.empty()) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    // The hit snapshot already applies clipping. Follow its actual ancestors,
    // handing off at a limit only within the same popup layer.
    for (auto current = std::optional{target->component}; current; current = services_->components().parent(*current)) {
        auto s = table_state(*services_, *current);
        if (s && services_->interactions().component_active(*current) &&
            contains(translated(s->body_bounds, services_->nodes().require(s->node).translation), event.x, event.y)) {
            const auto x = std::clamp(s->scroll_x + event.delta_x * 32.0F, 0.0F, s->maximum_x);
            const auto y = std::clamp(s->scroll_y - event.delta_y * 32.0F, 0.0F, s->maximum_y);
            if (x != s->scroll_x || y != s->scroll_y) {
                s->scroll_x = x;
                s->scroll_y = y;
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Transform | runtime::DirtyFlags::HitTest);
                return true;
            }
        }
        if (services_->components().is_window_layer_root(*current)) {
            return false;
        }
    }
    return false;
}

void TableComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                          std::optional<input::InteractionId> hit,
                                          std::optional<input::InteractionId>) {
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    for (auto id : mounted_) {
        auto s = table_state(*services_, id);
        if (!s) {
            continue;
        }
        std::optional<runtime::ComponentId> hovered;
        for (auto current = target && event.action != input::PointerAction::cancel ? std::optional{target->component}
                                                                                   : std::nullopt;
             current && services_->components().contains(*current);
             current = services_->components().parent(*current)) {
            if (std::ranges::find(s->visible, *current) != s->visible.end()) {
                hovered = current;
                break;
            }
        }
        if (s->hovered != hovered) {
            s->hovered = hovered;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
        }
    }
}

void TableComponentHost::on_window_active(bool value) {
    if (!value) {
        for (auto id : mounted_) {
            if (auto s = table_state(*services_, id)) {
                s->hovered.reset();
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
            }
        }
    }
}

void TableComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    for (auto id : mounted_) {
        auto s = table_state(*services_, id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& root = services_->nodes().require(s->node);
        const auto& token = services_->components().theme_scope(id)->snapshot().table();
        const auto clip =
            services_->nodes().content_clip(s->node, services_->component_clip(id, viewport, document_clip));
        std::vector<graphics::RoundedEffectInstance> quads;
        const auto quad = [&](runtime::Rect bounds, Color color, runtime::Point offset, runtime::Rect clipping) {
            for (auto effect : graphics::make_corner_fill_effects({bounds, 0}, {false, false, false, false}, color,
                                                                  services_->nodes().content_opacity(s->node), offset,
                                                                  graphics::EffectClip{1, clipping})) {
                quads.push_back(std::move(effect));
            }
        };
        quad(root.bounds, token.colors.background, root.translation, clip);
        static_cast<void>(services_->surfaces().update_content_effects(s->surface, quads));
        quads.clear();
        if (s->bordered) {
            const auto b = token.metrics.border_width;
            quad({root.bounds.x, root.bounds.y, root.bounds.width, b}, token.colors.border, root.translation, clip);
            quad({root.bounds.x, root.bounds.y, b, root.bounds.height}, token.colors.border, root.translation, clip);
            quad({root.bounds.x + std::max(0.0F, root.bounds.width - b), root.bounds.y, b, root.bounds.height},
                 token.colors.border, root.translation, clip);
            quad({root.bounds.x, root.bounds.y + std::max(0.0F, root.bounds.height - b), root.bounds.width, b},
                 token.colors.border, root.translation, clip);
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->border_surface, quads));
        if (s->loading) {
            quads.clear();
            const auto color = token.colors.background;
            quad(s->body_bounds, Color(color.red(), color.green(), color.blue(), 0.85F), root.translation, clip);
            static_cast<void>(services_->surfaces().update_content_effects(s->loading_surface, quads));
        }
        auto rows = s->visible;
        if (s->show_header) {
            rows.push_back(s->header);
        }
        for (auto row_id : rows) {
            const auto& row = *services_->components().state<RowState>(row_id);
            const auto& node = services_->nodes().require(row.node);
            const auto row_clip = services_->nodes().content_clip(row.node, clip);
            quads.clear();
            const auto background = row.header ? token.colors.header_background
                                    : row.checked.get()
                                        ? (s->hovered == row_id ? token.colors.selected_hover : token.colors.selected)
                                    : s->hovered == row_id ? token.colors.hover
                                                           : token.colors.background;
            quad(node.bounds, background, node.translation, row_clip);
            const auto b = token.metrics.border_width;
            quad({node.bounds.x, node.bounds.y + std::max(0.0F, node.bounds.height - b), node.bounds.width, b},
                 token.colors.border, node.translation, row_clip);
            if (s->bordered) {
                float x = node.bounds.x;
                for (auto width : s->widths) {
                    x += width;
                    quad({x - b, node.bounds.y, b, node.bounds.height}, token.colors.border, node.translation,
                         row_clip);
                }
            }
            static_cast<void>(services_->surfaces().update_content_effects(row.surface, quads));
        }
    }
}
} // namespace ryn::detail

namespace ryn::detail {
void mount_table(TableOptions options, Prop<std::vector<TableRecord>> source, std::vector<TableColumnSchema> columns,
                 std::optional<TableEmpty> empty) {
    if (!active_tables) {
        throw std::logic_error("Table requires window component services");
    }
    active_tables->mount(std::move(options), source, std::move(columns), empty);
}
} // namespace ryn::detail
