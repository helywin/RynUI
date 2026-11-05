#include "component/select_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/select_model.hpp"
#include "component/option_list_support.hpp"
#include "input/text_boundary.hpp"
#include <ryn/tag.hpp>
#include <ryn/flex.hpp>
#include "component/input_component.hpp"
#include "component/input_affix_action.hpp"
#include "component/popover_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local SelectComponentHost* active_selects{};

struct SelectRow final {
    runtime::NodeId node;
    input::InteractionId interaction;
    runtime::SceneFragmentId hit;
    component::RetainedSurfaceId surface;
    std::optional<std::size_t> index;
    String key;
    SelectRichContent rich;
    SelectOptionRenderer renderer;
    runtime::ComponentId label_root;
    Signal<bool> selected{false};
    Signal<bool> disabled{false};
    Signal<String> label{String{}};
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
};

struct SelectedTag final {
    SelectValue value;
    runtime::NodeId node;
    Signal<String> label{String{}};
    Signal<bool> disabled{false};
    Signal<bool> selected{true};
    bool overflow{};
    bool single{};
    SelectTagRenderer tag_renderer;
    SelectLabelRenderer label_renderer;
};

struct SelectRetireOnFailure final {
    WindowComponentServices& services;
    runtime::ComponentId id;
    bool complete{};

    ~SelectRetireOnFailure() noexcept {
        if (!complete) {
            (void)services.retire_failed_component(id);
        }
    }
};

struct SelectState final {
    explicit SelectState(SelectModel value) : model(std::move(value)) {}

    SelectModel model;
    runtime::NodeId node;
    runtime::ComponentId selector;
    runtime::ComponentId popup;
    runtime::ComponentId list;
    runtime::ComponentId clear_wrapper;
    input::InteractionId selector_hit;
    input::InteractionId popup_hit;
    input::TextInputOwnerId editor;
    component::RetainedSurfaceId background;
    component::RetainedSurfaceId clear_background;
    std::vector<runtime::ComponentId> rows;
    std::vector<runtime::ComponentId> parked;
    runtime::ComponentId selected_root;
    runtime::ComponentId popup_body;
    std::vector<runtime::ComponentId> tags;
    String overflow;
    SelectOptionRenderer option_renderer;
    SelectLabelRenderer label_renderer;
    SelectTagRenderer tag_renderer;
    SelectPopupRenderer popup_renderer;
    SelectSource source;
    std::shared_ptr<AsyncOptionsSession<SelectOption>> async{std::make_shared<AsyncOptionsSession<SelectOption>>()};
    OptionPopupMotion motion;
    bool animate{};
    Signal<TextDirection> direction{TextDirection::Auto};
    SelectMode mode{SelectMode::Single};
    Signal<bool> multiple{false};
    SelectFilter filter;
    SelectSort sort;
    SelectOptionField filter_field{SelectOptionField::Label};
    SelectOptionField label_field{SelectOptionField::Label};
    bool active_first{true};
    bool hide_selected{};
    bool auto_clear_search{true};
    std::size_t max_count{std::numeric_limits<std::size_t>::max()};
    std::size_t max_tag_count{std::numeric_limits<std::size_t>::max()};
    std::size_t max_tag_text{std::numeric_limits<std::size_t>::max()};
    bool responsive_tags{};
    bool explicit_read_only{};
    bool virtual_list{true};
    bool match_width{true};
    float popup_width{};
    SelectPlacement placement{SelectPlacement::BottomLeft};
    bool pending{};
    std::shared_ptr<Scope> bindings{std::make_shared<Scope>()};
    std::uint64_t model_revision{};
    bool mounting{true};
    bool reconciling{};
    bool tags_reconciling{};
    bool tags_dirty{};
    runtime::Size tag_size{-1, -1};
    bool rows_dirty{};
    bool show_search_explicit{};
    std::optional<Prop<SelectLabeledSelection>> labeled_authority;
    std::optional<Prop<SelectLabeledValues>> labeled_values_authority;
    std::optional<Prop<SelectSelection>> authority;
    std::optional<Prop<SelectValues>> values_authority;
    std::optional<Prop<String>> search_authority;
    std::vector<String> separators;
    std::function<std::vector<String>(StringView)> tokenizer;
    std::function<String(SelectValues)> max_tag_placeholder;
    std::function<void(SelectValues)> on_values_change;
    std::function<void(std::vector<SelectLabeledValue>)> on_labeled_change;
    std::function<void(SelectValue, SelectOption)> on_select;
    std::function<void(SelectValue, SelectOption)> on_deselect;
    std::function<void(SelectValue)> on_active;
    std::function<void()> on_clear;
    std::function<void(float)> on_scroll;
    std::function<void(String)> on_load_error;
    SelectRef reference;
    Signal<String> display{String{}};
    Signal<String> placeholder{String{}};
    String external_placeholder;
    String empty_label{u8"No data"};
    Signal<bool> read_only{true};
    Signal<bool> clear_visible{false};
    Signal<bool> action_disabled{false};
    Signal<LayoutGap> tag_gap{LayoutGap{dp(4)}};
    Signal<IconSource> clear_icon{IconSource{IconName::CloseCircleFilled}};
    Signal<IconSource> suffix_icon{IconSource{IconName::DownOutlined}};
    Signal<IconSource> search_icon{IconSource{IconName::SearchOutlined}};
    Signal<IconSource> loading_icon{IconSource{IconName::LoadingOutlined}};
    Signal<IconSource> remove_icon{IconSource{IconName::CloseOutlined}};
    Signal<IconSource> selected_icon{IconSource{IconName::CheckOutlined}};
    std::function<bool(SelectKeyEvent)> on_key;
    Signal<IconSource> arrow{IconSource{IconName::DownOutlined}};
    Signal<bool> spinning{false};
    Signal<ThemeConfig> selector_theme{ThemeConfig{}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    bool show_search{};
    bool allow_clear{};
    bool disabled{};
    bool loading{};
    bool zero_geometry{};
    bool focused{};
    bool focus_was_inside{};
    bool controlled_open{};
    bool open{};
    bool suppressed{};
    bool visible{};
    bool above{};
    bool reveal_active{true};
    bool needs_layout{true};
    std::uint64_t layout_generation{};
    runtime::Rect layout_anchor;
    runtime::Rect layout_clip;
    float list_height{256};
    float scroll{};
    runtime::Rect popup_bounds;
    std::function<void(SelectSelection)> on_change;
    std::function<void(String)> on_search;
    std::function<void(bool)> on_open;
    std::function<void()> on_focus;
    std::function<void()> on_blur;
};

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

graphics::QuadInstance quad(runtime::Rect bounds, Color color, float radius = 0) {
    graphics::QuadInstance result;
    result.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
    result.color = {color.red(), color.green(), color.blue(), color.alpha()};
    result.corner_radius = radius;
    return result;
}

runtime::Rect intersect(runtime::Rect a, runtime::Rect b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x),
            std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y)};
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

// A virtual row may own the active native editor. Move focus within the
// compound control before hiding it, so scrolling ends preedit without closing the list.
void restore_row_focus(WindowComponentServices& services, runtime::ComponentId owner, runtime::ComponentId row,
                       input::InteractionId editor) {
    const auto focused = services.focus().state().focused;
    const auto* record = focused ? services.interactions().find(*focused) : nullptr;
    if (record && descendant(services, record->component, row)) {
        SelectRetireOnFailure rollback{services, owner};
        services.focus().defer_focus(editor, input::FocusModality::keyboard);
        rollback.complete = true;
    }
}

SelectState* select_state(WindowComponentServices& services, runtime::ComponentId id) {
    auto* state = services.components().state<SelectState>(id);
    if (!state || !services.components().scope(id).active()) {
        return nullptr;
    }
    if (!services.nodes().find(state->node)) {
        (void)services.retire_failed_component(id);
        return nullptr;
    }
    return state;
}

template <class F>
std::optional<bool> change_select_model(WindowComponentServices& services, runtime::ComponentId id, F operation) {
    const auto* state = select_state(services, id);
    if (!state) {
        return {};
    }
    const auto revision = state->model_revision;
    auto candidate = state->model;
    const bool changed = operation(candidate);
    auto* current = select_state(services, id);
    if (!current || current->model_revision != revision) {
        return {};
    }
    if (changed) {
        current->model = std::move(candidate);
        ++current->model_revision;
    }
    return changed;
}

template <class F, class... Args>
void invoke_select_callback(WindowComponentServices& services, runtime::ComponentId id, const F& function,
                            Args&&... args) {
    if (!select_state(services, id)) {
        return;
    }
    SelectRetireOnFailure rollback{services, id};
    const auto callback = function;
    if (callback) {
        callback(std::forward<Args>(args)...);
    }
    rollback.complete = true;
}

runtime::ComponentId select_group(WindowComponentServices& services, bool leaf = false) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    if (leaf) {
        services.layout().set_layout(node, layout::LeafLayout{});
    } else {
        services.layout().set_layout(node, layout::BoxLayout{});
    }
    build.on_resource_cleanup(id, [&services, id, node] {
        services.interactions().set_input_blocked(id, false);
        services.layout().remove_layout(node);
    });
    return id;
}

std::vector<std::size_t> graphemes(StringView text) {
    input::TextBoundaryMap map;
    if (!map.assign(text.bytes())) {
        throw std::invalid_argument("Select text is not UTF-8");
    }
    return {map.grapheme_bytes().begin(), map.grapheme_bytes().end()};
}

void validate_height(float height) {
    if (!std::isfinite(height) || height <= 0) {
        throw std::invalid_argument("Select listHeight must be positive and finite");
    }
}
} // namespace

SelectComponentHost::SelectComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SelectComponentHost::~SelectComponentHost() {
    services_->detach(*this);
}

void* SelectComponentHost::begin_mount() noexcept {
    ++mount_depth_;
    auto* previous = active_selects;
    active_selects = this;
    return previous;
}

void SelectComponentHost::end_mount(void* previous) noexcept {
    --mount_depth_;
    active_selects = static_cast<SelectComponentHost*>(previous);
}

void SelectComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

SelectSnapshot SelectComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = select_state(*services_, id);
    if (!s) {
        throw std::out_of_range("Select component is stale");
    }
    SelectSnapshot result{s->selector,      s->popup_bounds,   s->visible, s->above, s->model.value(),
                          s->model.query(), s->model.active(), s->rows,    s->scroll};
    result.values = s->model.values();
    result.total_rows = s->model.filtered().size();
    result.pending = s->pending;
    result.tags = s->tags;
    result.overflow = s->overflow;
    for (auto child : s->rows) {
        if (auto* row = services_->components().state<SelectRow>(child); row && row->index) {
            result.row_indices.push_back(*row->index);
        } else {
            result.row_indices.push_back(std::numeric_limits<std::size_t>::max());
        }
    }
    return result;
}

void SelectComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = select_state(*services_, id);
    if (!s || s->mounting) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    if (s->mode == SelectMode::Tags && !s->reconciling) {
        rebuild_rows(id);
        rollback.complete = true;
        return;
    }
    const bool visible = s->open && !s->suppressed && !s->disabled && !s->explicit_read_only && !s->zero_geometry &&
                         window_active_ && services_->interactions().component_active(id);
    const bool changed = s->visible != visible;
    s->visible = visible;
    if (changed) {
        if (visible) {
            s->motion.begin(services_->components().theme_scope(id)->snapshot(), services_->motion_preference(),
                            services_->animation_time(), s->animate);
            if (s->motion.start) {
                services_->request_frame();
            }
        } else {
            s->motion.stop();
        }
    }
    services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
    if (services_->components().set_branch_active(s->popup, visible)) {
        services_->mark_scene_structure_dirty();
    }
    const bool editing = visible && s->show_search;
    const bool multi = s->mode != SelectMode::Single;
    const auto display = s->display;
    const auto placeholder = s->placeholder;
    const auto read_only = s->read_only;
    const auto clear_visible = s->clear_visible;
    const auto action_disabled = s->action_disabled;
    const auto arrow = s->arrow;
    const auto spinning = s->spinning;
    display.set(editing ? s->model.query() : multi || s->label_renderer ? String{} : s->model.label());
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    placeholder.set(!multi && editing && s->model.value()
                        ? s->model.label()
                        : (multi && !s->model.values().empty() ? String{} : s->external_placeholder));
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    read_only.set(!s->show_search || s->explicit_read_only);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    clear_visible.set(s->allow_clear && (multi ? !s->model.values().empty() : s->model.value().has_value()) &&
                      !s->disabled && !s->loading && !s->explicit_read_only);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    action_disabled.set(s->disabled || s->loading || s->explicit_read_only);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    arrow.set(s->loading ? s->loading_icon.get() : editing ? s->search_icon.get() : s->suffix_icon.get());
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    spinning.set(s->loading);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    reconcile_tags(id);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    const auto token = services_->components().theme_scope(id)->snapshot().select();
    const auto rows = s->rows;
    const auto filtered = std::vector<std::size_t>{s->model.filtered().begin(), s->model.filtered().end()};
    for (auto child : rows) {
        auto* row = services_->components().state<SelectRow>(child);
        if (!row) {
            continue;
        }
        row->index = s->model.index(row->key.view());
        const auto* found = row->index ? &s->model.options()[*row->index] : nullptr;
        const bool shown = (!row->index && row->key == String{u8"$empty"} && filtered.empty()) ||
                           (row->index && std::ranges::find(filtered, *row->index) != filtered.end());
        const bool selected = row->index && s->model.selected(found->value);
        const bool disabled = !row->index || found->disabled || found->group || s->disabled || s->loading ||
                              s->explicit_read_only || (multi && !selected && s->model.values().size() >= s->max_count);
        if (services_->components().set_branch_active(child, shown)) {
            services_->mark_scene_structure_dirty();
        }
        services_->interactions().set_eligible(row->interaction, visible && shown && !disabled);
        const auto label = row->label;
        const auto color = row->foreground;
        const auto font = row->typography;
        const auto selected_signal = row->selected;
        const auto disabled_signal = row->disabled;
        const auto text = row->index ? found->label : s->empty_label;
        const auto& node = services_->nodes().require(row->node);
        const auto menu_clip = services_->nodes().require(services_->components().root(s->list)).bounds;
        const bool on_screen =
            node.place_count == 0 || menu_clip.height <= 0 || intersect(node.bounds, menu_clip).height > 0;
        services_->set_input_blocked(child, !visible || !shown || !on_screen || disabled);
        if (!select_state(*services_, id)) {
            return;
        }
        label.set(text);
        if (!select_state(*services_, id)) {
            return;
        }
        color.set(foreground(disabled   ? token.colors.option_disabled_color
                             : selected ? token.colors.option_selected_color
                                        : token.colors.option_color));
        if (!select_state(*services_, id)) {
            return;
        }
        font.set({token.typography.font_family,
                  selected ? token.typography.selected_font_weight : token.typography.font_weight, false,
                  token.typography.font_size, token.typography.line_height});
        if (!select_state(*services_, id)) {
            return;
        }
        selected_signal.set(selected);
        if (!select_state(*services_, id)) {
            return;
        }
        disabled_signal.set(disabled);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
    }
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (geometry || changed) {
        s->needs_layout = true;
        s->reveal_active = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    } else {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    }
    services_->set_input_blocked(s->popup, !visible);
    rollback.complete = true;
}

void SelectComponentHost::request_open(runtime::ComponentId id, bool open) {
    auto* s = select_state(*services_, id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || s->explicit_read_only || !window_active_ ||
                  !services_->interactions().component_active(id)))) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    const bool changed = s->open != open;
    const auto callback = s->on_open;
    if (!s->controlled_open) {
        s->open = open;
    }
    if (open) {
        s->suppressed = false;
    }
    if (!open) {
        auto async = s->async;
        s->pending = false;
        async->cancel();
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        const auto query = s->search_authority ? read_prop(*s->search_authority) : String{};
        if (!change_select_model(*services_, id, [&](auto& model) {
                model.search(query);
                return true;
            })) {
            return;
        }
    }
    refresh(id);
    if (changed && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(open);
    }
    rollback.complete = true;
}

void SelectComponentHost::choose(runtime::ComponentId id, std::optional<std::size_t> index) {
    auto* s = select_state(*services_, id);
    if (!s || s->disabled || s->loading || s->explicit_read_only) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    if (index && *index < s->model.options().size()) {
        const auto key = SelectModel::key(s->model.options()[*index]);
        const auto focus = services_->focus().state().focused;
        const auto* focused = focus ? services_->interactions().find(*focus) : nullptr;
        if (focused && descendant(*services_, focused->component, s->popup)) {
            services_->focus().defer_focus(s->selector_hit, input::FocusModality::keyboard);
        }
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        index = s->model.index(key.view());
        if (!index) {
            rollback.complete = true;
            return;
        }
    }
    if (s->mode != SelectMode::Single) {
        auto candidate = s->model;
        const auto intent = candidate.request_values(index);
        if (intent && select_state(*services_, id)) {
            publish_values(id, *intent, !index);
        }
        rollback.complete = true;
        return;
    }
    const auto previous = s->model.value();
    const auto option =
        index && *index < s->model.options().size() ? std::optional{s->model.options()[*index]} : std::nullopt;
    auto intent = s->model.request(index);
    if (!intent) {
        rollback.complete = true;
        return;
    }
    const auto callback = s->on_change;
    const auto selected = s->on_select;
    const auto clear = s->on_clear;
    const auto labeled = s->on_labeled_change;
    const auto query = s->search_authority ? read_prop(*s->search_authority) : String{};
    if (!change_select_model(*services_, id, [&](auto& model) {
            model.search(query);
            return true;
        })) {
        rollback.complete = true;
        return;
    }
    refresh(id);
    if (select_state(*services_, id) && previous != *intent && callback) {
        callback(*intent);
    }
    s = select_state(*services_, id);
    if (!s) {
        rollback.complete = true;
        return;
    }
    if (labeled && previous != *intent) {
        std::vector<SelectLabeledValue> values;
        if (*intent) {
            values.push_back({**intent, s->model.label(**intent)});
        }
        labeled(std::move(values));
    }
    if (select_state(*services_, id) && option && selected) {
        selected(option->value, *option);
    }
    if (select_state(*services_, id) && !index && clear) {
        clear();
    }
    if (select_state(*services_, id)) {
        request_open(id, false);
    }
    rollback.complete = true;
}

void SelectComponentHost::publish_values(runtime::ComponentId id, SelectValues intent, bool clear) {
    auto* s = select_state(*services_, id);
    if (!s || s->disabled || s->loading || s->explicit_read_only) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    const auto previous = s->values_authority ? read_prop(*s->values_authority) : s->model.values();
    const auto callback = s->on_values_change;
    const auto selected = s->on_select;
    const auto deselected = s->on_deselect;
    const auto labeled = s->on_labeled_change;
    const auto on_clear = s->on_clear;
    // Requests from model toggles may already update uncontrolled state.
    if (!change_select_model(*services_, id, [&](auto& model) {
            model.accept_values(intent);
            return true;
        })) {
        rollback.complete = true;
        return;
    }
    if (callback && previous != intent) {
        callback(intent);
    }
    s = select_state(*services_, id);
    if (!s) {
        rollback.complete = true;
        return;
    }
    if (s->values_authority) {
        const auto values = read_prop(*s->values_authority);
        if (!change_select_model(*services_, id, [&](auto& model) { return model.reconcile_values(values); })) {
            rollback.complete = true;
            return;
        }
        s = select_state(*services_, id);
        if (!s) {
            rollback.complete = true;
            return;
        }
    }
    std::vector<SelectLabeledValue> labels;
    for (const auto& value : intent) {
        labels.push_back({value, s->model.label(value)});
    }
    const auto options = s->model;
    if (labeled && previous != intent) {
        labeled(std::move(labels));
    }
    for (const auto& value : intent) {
        if (!select_state(*services_, id)) {
            rollback.complete = true;
            return;
        }
        if (std::ranges::find(previous, value) == previous.end() && selected) {
            selected(value, options.option(value));
        }
    }
    for (const auto& value : previous) {
        if (!select_state(*services_, id)) {
            rollback.complete = true;
            return;
        }
        if (std::ranges::find(intent, value) == intent.end() && deselected && !clear) {
            deselected(value, options.option(value));
        }
    }
    if (clear && select_state(*services_, id) && on_clear) {
        on_clear();
    }
    s = select_state(*services_, id);
    if (!s) {
        rollback.complete = true;
        return;
    }
    if (s->auto_clear_search || clear) {
        const auto query = s->search_authority ? read_prop(*s->search_authority) : String{};
        if (!change_select_model(*services_, id, [&](auto& model) {
                model.search(query);
                return true;
            })) {
            rollback.complete = true;
            return;
        }
    }
    refresh(id);
    rollback.complete = true;
}

void SelectComponentHost::edit(runtime::ComponentId id, String query) {
    auto* s = select_state(*services_, id);
    if (!s || !s->show_search || s->disabled || s->explicit_read_only) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    const auto callback = s->on_search;
    if (s->mode != SelectMode::Single) {
        std::vector<String> tokens;
        String remainder = query;
        if (s->tokenizer) {
            const auto tokenize = s->tokenizer;
            tokens = tokenize(query.view());
            if (tokens.empty() || (tokens.size() == 1 && tokens.front() == query)) {
                tokens.clear();
            } else {
                remainder = {};
            }
        } else {
            const auto bytes = query.bytes();
            std::size_t start = 0;
            for (std::size_t p = 0; p < bytes.size();) {
                const auto found = std::ranges::find_if(s->separators, [&](const auto& separator) {
                    return !separator.empty() && bytes.substr(p).starts_with(separator.bytes());
                });
                if (found == s->separators.end()) {
                    ++p;
                    continue;
                }
                tokens.push_back(String::from_utf8(bytes.substr(start, p - start)).value());
                p += found->bytes().size();
                start = p;
            }
            remainder = String::from_utf8(bytes.substr(start)).value();
        }
        s = select_state(*services_, id);
        if (!s) {
            rollback.complete = true;
            return;
        }
        auto candidate = s->model;
        if (const auto intent = candidate.request_tokens(tokens); intent && select_state(*services_, id)) {
            publish_values(id, *intent);
        }
        s = select_state(*services_, id);
        if (!s) {
            rollback.complete = true;
            return;
        }
        query = std::move(remainder);
    }
    const auto accepted = s->search_authority ? read_prop(*s->search_authority) : query;
    if (!change_select_model(*services_, id, [&](auto& model) {
            model.search(accepted);
            return true;
        })) {
        rollback.complete = true;
        return;
    }
    request_open(id, true);
    if (select_state(*services_, id) && callback) {
        callback(query);
    }
    if (select_state(*services_, id)) {
        load(id, query);
    }
    rollback.complete = true;
}

void SelectComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = select_state(*services_, id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto& token = theme.select();
    ThemeConfig config;
    config.input.tokens.background = token.colors.selector_background;
    config.input.tokens.hover_background = token.colors.selector_background;
    config.input.tokens.active_background = token.colors.selector_background;
    config.input.tokens.hover_border_color = token.colors.hover_border_color;
    config.input.tokens.active_border_color = token.colors.active_border_color;
    const auto& original = InputTokenAccess::get(theme).active_shadow;
    std::array<ShadowLayer, ShadowList::capacity> shadow{};
    std::copy(original.layers().begin(), original.layers().end(), shadow.begin());
    for (std::size_t i = 0; i < original.size(); ++i) {
        shadow[i].color = token.colors.active_outline_color;
    }
    config.input.tokens.active_shadow = ShadowList{std::span<const ShadowLayer>{shadow.data(), original.size()}};
    auto tag_gap = s->tag_gap;
    tag_gap.set(LayoutGap{dp(token.metrics.gap)});
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (geometry && s->selected_root.valid()) {
        layout::FlexLayout flex;
        flex.right_to_left = s->direction.get() == TextDirection::RightToLeft;
        flex.align = layout::FlexAlign::center;
        flex.main_gap = token.metrics.gap;
        services_->layout().set_layout(services_->components().root(s->selected_root), flex);
        const auto prefix = services_->components().parent(s->selected_root);
        flex.item_policy = layout::FlexItemPolicy::sequential;
        services_->layout().set_layout(services_->components().root(*prefix), flex);
    }
    auto selector_theme = s->selector_theme;
    selector_theme.set(std::move(config));
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (services_->components().set_window_layer(s->popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    refresh(id, geometry);
}

void SelectComponentHost::mount_row(runtime::ComponentId id, std::optional<std::size_t> index) {
    auto& services = *services_;
    auto* s = select_state(services, id);
    if (!s) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    const auto row_id = build.mount_component<SelectRow>();
    auto& row = build.state<SelectRow>(row_id);
    row.node = build.root(row_id);
    row.index = index;
    row.key = index ? SelectModel::key(s->model.options()[*index]) : String{u8"$empty"};
    row.rich = index ? s->model.options()[*index].rich_label : SelectRichContent{};
    row.renderer = s->option_renderer;
    row.label.set(row.index ? s->model.options()[*index].label : s->empty_label);
    services.layout().set_layout(row.node, layout::BoxLayout{});
    row.interaction = services.interactions().create({row_id, row.node, s->popup_hit, false, false, {}, false, false});
    input::InteractionHandlers handlers;
    handlers.target = [this, id, row_id](input::PointerDispatchContext& event) {
        auto* s = select_state(*services_, id);
        auto* row = services_->components().state<SelectRow>(row_id);
        if (!s || !row || !row->index || s->disabled || s->loading || s->explicit_read_only || !s->visible) {
            return;
        }
        const auto index = *row->index;
        if (event.kind() == input::PointerEventKind::enter || event.kind() == input::PointerEventKind::move) {
            if (s->model.activate(index)) {
                const auto callback = s->on_active;
                const auto value = s->model.options()[index].value;
                refresh(id, false);
                if (select_state(*services_, id) && callback) {
                    invoke_select_callback(*services_, id, callback, value);
                }
            }
        }
        if (event.kind() == input::PointerEventKind::down && event.event().button == input::PointerButton::primary) {
            static_cast<void>(event.capture_pointer());
            event.stop_propagation();
        }
        if (event.kind() == input::PointerEventKind::up && event.event().button == input::PointerButton::primary) {
            static_cast<void>(event.release_pointer_capture());
            const auto bounds = services_->nodes().require(row->node).bounds;
            const bool hit = event.press_origin() == row->interaction &&
                             inside(intersect(bounds, s->popup_bounds), event.event().x, event.event().y);
            event.stop_propagation();
            if (hit) {
                choose(id, index);
            }
        }
    };
    services.interactions().set_handlers(row.interaction, std::move(handlers));
    row.hit = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
    graphics::QuadInstance initial;
    initial.opacity = 0;
    component::RetainedSurfaceEffects effects;
    effects.focus_enabled = false;
    services.scene_composer().set_fragment(row.hit, {}, row.interaction);
    const auto body = build.register_scene_fragment(row_id, runtime::SceneFragmentPlacement::before_children);
    row.surface = services.surfaces().create_surface(row_id, row.node, body, std::span{&initial, 1}, effects);
    build.on_resource_cleanup(
        row_id, [&services, row_id, node = row.node, hit = row.interaction, fragment = row.hit, surface = row.surface] {
            services.interactions().set_input_blocked(row_id, false);
            services.pointer().cancel_interaction(hit);
            services.interactions().remove(hit);
            services.surfaces().destroy(surface);
            services.scene_composer().remove_fragment(fragment);
            services.layout().remove_layout(node);
        });
    s = select_state(services, id);
    if (!s) {
        return;
    }
    s->rows.push_back(row_id);
    build.mount_slot(row_id, Content{[this, id, row_id] { mount_label(id, row_id); }});
}

void SelectComponentHost::mount_label(runtime::ComponentId id, runtime::ComponentId child) {
    const auto* s = select_state(*services_, id);
    auto* row = services_->components().state<SelectRow>(child);
    if (!s || !row) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    row->label_root = select_group(*services_);
    const auto root = row->label_root;
    const auto rich = row->rich;
    const auto render = row->renderer;
    const auto label = row->label;
    const auto selected = row->selected;
    const auto disabled = row->disabled;
    const auto direction = s->direction;
    const auto value = row->index ? s->model.options()[*row->index].value : SelectValue{};
    const auto selected_icon = s->selected_icon;
    const auto multiple = s->multiple;
    build.mount_slot_with_semantic_text_style(
        root, Content{[=] {
            Flex(FlexProps{}
                     .direction(bind([direction] {
                         return direction.get() == TextDirection::RightToLeft ? FlexDirection::RightToLeft
                                                                              : FlexDirection::LeftToRight;
                     }))
                     .align(FlexAlign::Center)
                     .justify(FlexJustify::SpaceBetween),
                 FlexContent{[=] {
                     if (render) {
                         render({value, label, selected, disabled, {}});
                     } else if (const auto* content = rich.content()) {
                         SlotContentAccess::function (*content)();
                     } else {
                         Text(TextProps{}.content(label).direction(direction));
                     }
                     Icon(IconProps{}.source(selected_icon).visible(bind([multiple, selected] {
                         return multiple.get() && selected.get();
                     })));
                 }});
        }},
        row->foreground, row->typography);
    // Keep pointer-operable rich controls in the compound control's single Tab boundary.
    for (auto hit : services_->interactions().declaration_order()) {
        const auto* record = services_->interactions().find(hit);
        if (record && descendant(*services_, record->component, child)) {
            services_->interactions().set_tab_stop(hit, false);
        }
    }
}

void SelectComponentHost::mount_rows(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s) {
        return;
    }
    std::vector<std::optional<std::size_t>> desired;
    if (!s->virtual_list || !s->match_width || s->model.options().size() <= 32) {
        for (std::size_t i = 0; i < s->model.options().size(); ++i) {
            desired.push_back(i);
        }
        desired.push_back(std::nullopt);
    } else {
        const auto filtered = s->model.filtered();
        const auto height = services_->components().theme_scope(id)->snapshot().select().metrics.option_height;
        const auto range = windowed_range(filtered.size(), s->scroll, s->list_height, height, true);
        for (auto i = range.first; i < range.end; ++i) {
            desired.push_back(filtered[i]);
        }
        if (filtered.empty()) {
            desired.push_back(std::nullopt);
        }
    }
    for (const auto index : desired) {
        mount_row(id, index);
    }
}

void SelectComponentHost::rebuild_rows(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s || s->mounting) {
        return;
    }
    if (s->reconciling) {
        s->rows_dirty = true;
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    s->reconciling = true;
    do {
        s->rows_dirty = false;
        const auto options = s->model.options();
        std::vector<std::optional<std::size_t>> desired;
        if (!s->virtual_list || !s->match_width || options.size() <= 32) {
            for (std::size_t i = 0; i < options.size(); ++i) {
                desired.push_back(i);
            }
            desired.push_back(std::nullopt);
        } else {
            const auto filtered = s->model.filtered();
            const auto height = services_->components().theme_scope(id)->snapshot().select().metrics.option_height;
            const auto range = windowed_range(filtered.size(), s->scroll, s->list_height, height, true);
            for (auto i = range.first; i < range.end; ++i) {
                desired.push_back(filtered[i]);
            }
            if (filtered.empty()) {
                desired.push_back(std::nullopt);
            }
        }
        auto previous = s->rows;
        previous.insert(previous.end(), s->parked.begin(), s->parked.end());
        s->parked.clear();
        std::vector<runtime::ComponentId> next;
        for (const auto index : desired) {
            const auto key = index ? SelectModel::key(options[*index]) : String{u8"$empty"};
            const auto rich = index ? options[*index].rich_label : SelectRichContent{};
            const auto found = std::ranges::find_if(previous, [&](auto child) {
                const auto* row = services_->components().state<SelectRow>(child);
                return row && row->key == key;
            });
            if (found == previous.end()) {
                const auto list = s->list;
                const Content content{[this, id, index] { mount_row(id, index); }};
                if (mount_depth_) {
                    runtime::require_component_build_context().mount_slot(list, content);
                } else {
                    services_->append_slot(list, content);
                }
                s = select_state(*services_, id);
                if (!s) {
                    return;
                }
                next.push_back(s->rows.back());
            } else {
                const auto child = *found;
                auto* row = services_->components().state<SelectRow>(child);
                row->index = index;
                if (row->rich != rich || row->renderer != s->option_renderer) {
                    const auto old = row->label_root;
                    row->rich = rich;
                    row->renderer = s->option_renderer;
                    row->label_root = {};
                    services_->set_input_blocked(old, true);
                    if (!select_state(*services_, id)) {
                        return;
                    }
                    services_->destroy(old);
                    if (!select_state(*services_, id)) {
                        return;
                    }
                    services_->append_slot(child, Content{[this, id, child] { mount_label(id, child); }});
                    s = select_state(*services_, id);
                    if (!s) {
                        return;
                    }
                }
                next.push_back(child);
                previous.erase(found);
            }
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
            if (s->rows_dirty) {
                break;
            }
        }
        if (s->rows_dirty) {
            continue;
        }
        s->rows = std::move(next);
        for (auto child : previous) {
            const auto* row = services_->components().state<SelectRow>(child);
            const bool retained =
                row && (row->rich.content() || row->renderer) && s->model.index(row->key.view()).has_value();
            if (retained) {
                s->parked.push_back(child);
                restore_row_focus(*services_, id, child, s->selector_hit);
                s = select_state(*services_, id);
                if (!s) {
                    return;
                }
                row = services_->components().state<SelectRow>(child);
                if (!row) {
                    return;
                }
                services_->components().set_branch_active(child, false);
                services_->interactions().set_eligible(row->interaction, false);
                services_->set_input_blocked(child, true);
            } else {
                services_->set_input_blocked(child, true);
                services_->destroy(child);
            }
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
        }
        refresh(id);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
    } while (s->rows_dirty);
    s->reconciling = false;
    rollback.complete = true;
}

void SelectComponentHost::reconcile_tags(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s || s->mounting || !s->selected_root.valid()) {
        return;
    }
    if (s->tags_reconciling) {
        s->tags_dirty = true;
        return;
    }
    s->tags_reconciling = true;
    s->tags_dirty = false;
    bool geometry_changed = false;
    SelectRetireOnFailure rollback{*services_, id};
    SelectValues values =
        s->mode == SelectMode::Single
            ? (s->label_renderer && s->model.value() ? SelectValues{*s->model.value()} : SelectValues{})
            : s->model.values();
    const bool single_mode = s->mode == SelectMode::Single;
    const bool responsive = s->responsive_tags && !single_mode;
    std::size_t visible = single_mode ? values.size() : std::min(values.size(), s->max_tag_count);
    if (responsive) {
        visible = 0;
    }
    SelectValues omitted;
    if (visible < values.size()) {
        omitted.assign(values.begin() + static_cast<std::ptrdiff_t>(visible), values.end());
    }
    const auto placeholder = s->max_tag_placeholder;
    const auto overflow_text = omitted.empty() ? String{}
                               : placeholder
                                   ? placeholder(omitted)
                                   : String::from_utf8("+ " + std::to_string(omitted.size()) + " ...").value();
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    s->overflow = overflow_text;
    auto& selected_node = services_->nodes().require(services_->components().root(s->selected_root));
    if (!responsive && selected_node.external_layout.max_width) {
        selected_node.external_layout.max_width.reset();
        geometry_changed = true;
    }
    auto previous = s->tags;
    std::vector<runtime::ComponentId> next;
    for (std::size_t i = 0; i < values.size() + (!single_mode && !values.empty() ? 1 : 0); ++i) {
        const bool overflow = i == values.size();
        const auto value = overflow ? SelectValue{} : values[i];
        const auto found = std::ranges::find_if(previous, [&](auto child) {
            const auto* tag = services_->components().state<SelectedTag>(child);
            return tag && tag->overflow == overflow && (overflow || tag->value == value) &&
                   tag->single == (s->mode == SelectMode::Single) &&
                   (overflow ||
                    (tag->single ? tag->label_renderer == s->label_renderer : tag->tag_renderer == s->tag_renderer));
        });
        runtime::ComponentId child;
        if (found != previous.end()) {
            child = *found;
            previous.erase(found);
        } else {
            const auto parent = s->selected_root;
            const auto tag_renderer = s->tag_renderer;
            const auto label_renderer = s->label_renderer;
            const bool single = s->mode == SelectMode::Single;
            const Content content{[this, id, value, overflow, tag_renderer, label_renderer, single, &child] {
                auto& build = runtime::require_component_build_context();
                child = build.mount_component<SelectedTag>();
                auto& tag = build.state<SelectedTag>(child);
                tag.value = value;
                tag.overflow = overflow;
                tag.node = build.root(child);
                services_->interactions().set_input_blocked(child, true);
                tag.single = single;
                tag.tag_renderer = tag_renderer;
                tag.label_renderer = label_renderer;
                services_->layout().set_layout(tag.node, layout::BoxLayout{});
                build.on_resource_cleanup(child, [services = services_, node = tag.node, child] {
                    services->interactions().set_input_blocked(child, false);
                    services->layout().remove_layout(node);
                });
                const auto label = tag.label;
                const auto disabled = tag.disabled;
                const auto selected = tag.selected;
                const auto remove = [this, id, value] {
                    auto* s = select_state(*services_, id);
                    if (!s || s->disabled || s->loading || s->explicit_read_only) {
                        return;
                    }
                    auto values = s->model.values();
                    std::erase(values, value);
                    publish_values(id, std::move(values));
                };
                const auto direction = select_state(*services_, id)->direction;
                const auto remove_icon = select_state(*services_, id)->remove_icon;
                const auto tag_gap = select_state(*services_, id)->tag_gap;
                auto* services = services_;
                build.mount_slot(
                    child, Content{[=] {
                        if (!overflow && single && label_renderer) {
                            label_renderer({value, label, selected, disabled, {}});
                        } else if (!overflow && !single && tag_renderer) {
                            tag_renderer({value, label, selected, disabled, remove});
                        } else if (single) {
                            Text(TextProps{}.content(label).direction(direction));
                        } else {
                            Tag(TagProps{}.visible(true).disabled(disabled), TagContent{[=] {
                                    Flex(FlexProps{}.align(FlexAlign::Center).gap(tag_gap).direction(bind([direction] {
                                        return direction.get() == TextDirection::RightToLeft
                                                   ? FlexDirection::RightToLeft
                                                   : FlexDirection::LeftToRight;
                                    })),
                                         FlexContent{[=] {
                                             Text(TextProps{}.content(label).direction(direction));
                                             if (!overflow) {
                                                 mount_input_affix_action(*services, remove_icon, disabled, remove,
                                                                          true, false, false);
                                             }
                                         }});
                                }});
                        }
                    }});
            }};
            if (mount_depth_) {
                runtime::require_component_build_context().mount_slot(parent, content);
            } else {
                services_->append_slot(parent, content);
            }
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
        }
        auto* tag = services_->components().state<SelectedTag>(child);
        if (!tag) {
            continue;
        }
        auto label = tag->label;
        auto disabled = tag->disabled;
        String text = overflow ? s->overflow : s->model.label(value);
        if (!overflow) {
            const auto boundaries = graphemes(text.view());
            if (boundaries.size() - 1 > s->max_tag_text) {
                text =
                    String::from_utf8(std::string{text.bytes().substr(0, boundaries[s->max_tag_text])} + "...").value();
            }
        }
        const bool tag_disabled =
            s->disabled || s->loading || s->explicit_read_only || (!overflow && s->model.option(value).disabled);
        if (!responsive) {
            const bool shown = overflow ? !omitted.empty() : i < visible;
            auto& tag_node = services_->nodes().require(tag->node);
            if (tag_node.out_of_flow != !shown) {
                tag_node.out_of_flow = !shown;
                geometry_changed = true;
            }
            if (services_->components().set_branch_active(child, shown)) {
                services_->mark_scene_structure_dirty();
                geometry_changed = true;
            }
            services_->set_input_blocked(child, !shown || tag_disabled);
        }
        if (!select_state(*services_, id)) {
            return;
        }
        label.set(std::move(text));
        if (!select_state(*services_, id)) {
            return;
        }
        disabled.set(tag_disabled);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        next.push_back(child);
    }
    if (responsive && !values.empty()) {
        const auto bounds = services_->nodes().require(s->node).bounds;
        const auto input_tokens = InputTokenAccess::get(services_->components().theme_scope(s->selector)->snapshot());
        const auto input_size = input_tokens.size(services_->input_runtime()->size(s->selector));
        const auto gap = services_->components().theme_scope(id)->snapshot().select().metrics.gap;
        float budget = 0;
        auto& engine = services_->layout();
        if (bounds.width > 0 && bounds.height > 0) {
            const auto main = services_->components().root(s->selector);
            const auto input_children = services_->nodes().require(main).children;
            const auto tags_node = services_->components().root(s->selected_root);
            const auto was_out = services_->nodes().require(tags_node).out_of_flow;
            services_->nodes().require(tags_node).out_of_flow = true;
            const auto prefix_size = engine.measure_child(input_children[0], {0, bounds.width, 0, bounds.height});
            if (!select_state(*services_, id)) {
                return;
            }
            services_->nodes().require(tags_node).out_of_flow = was_out;
            const auto suffix_size = engine.measure_child(input_children[2], {0, bounds.width, 0, bounds.height});
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
            budget = std::max(
                0.0F, bounds.width - prefix_size.width - suffix_size.width -
                          2 * (input_size.padding_inline + input_tokens.border_width + input_tokens.affix_padding) -
                          input_size.font_size);
        }
        auto& selected = services_->nodes().require(services_->components().root(s->selected_root));
        if (selected.external_layout.max_width != std::optional<float>{budget}) {
            selected.external_layout.max_width = budget;
            geometry_changed = true;
        }
        selected.clip_content = true;
        const auto overflow = next.back();
        auto* overflow_state = services_->components().state<SelectedTag>(overflow);
        const auto overflow_node = overflow_state->node;
        auto overflow_label = overflow_state->label;
        std::size_t best = 0;
        String best_label = s->overflow;
        float sum = 0;
        const auto maximum = std::min(values.size(), s->max_tag_count);
        for (std::size_t count = 0; count <= maximum; ++count) {
            const bool omitted = count < values.size();
            String text;
            if (omitted) {
                const SelectValues hidden{values.begin() + static_cast<std::ptrdiff_t>(count), values.end()};
                text = placeholder ? placeholder(hidden)
                                   : String::from_utf8("+ " + std::to_string(hidden.size()) + " ...").value();
            }
            if (!select_state(*services_, id)) {
                return;
            }
            overflow_label.set(text);
            if (!select_state(*services_, id)) {
                return;
            }
            const auto overflow_width =
                omitted ? engine
                              .measure_child(overflow_node, {0, std::numeric_limits<float>::infinity(), 0,
                                                             std::max(0.0F, bounds.height)})
                              .width
                        : 0;
            const float width = sum + (count > 0 ? gap * static_cast<float>(count - 1) : 0) +
                                (omitted && count > 0 ? gap : 0) + overflow_width;
            if (width <= budget + 0.01F) {
                best = count;
                best_label = text;
            }
            if (count == maximum) {
                break;
            }
            const auto* tag = services_->components().state<SelectedTag>(next[count]);
            sum += engine
                       .measure_child(tag->node,
                                      {0, std::numeric_limits<float>::infinity(), 0, std::max(0.0F, bounds.height)})
                       .width;
            if (!select_state(*services_, id)) {
                return;
            }
            if (sum > budget) {
                break;
            }
        }
        overflow_label.set(best_label);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        s->overflow = best_label;
        for (std::size_t i = 0; i < next.size(); ++i) {
            const auto child = next[i];
            auto* tag = services_->components().state<SelectedTag>(child);
            const bool shown = tag->overflow ? best < values.size() : i < best;
            auto& node = services_->nodes().require(tag->node);
            if (node.out_of_flow != !shown) {
                node.out_of_flow = !shown;
                geometry_changed = true;
            }
            if (services_->components().set_branch_active(child, shown)) {
                services_->mark_scene_structure_dirty();
                geometry_changed = true;
            }
            services_->set_input_blocked(child, !shown || tag->disabled.get());
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
        }
    }
    s->tags = std::move(next);
    for (auto child : previous) {
        geometry_changed = true;
        services_->set_input_blocked(child, true);
        services_->destroy(child);
        if (!select_state(*services_, id)) {
            return;
        }
    }
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (responsive && !values.empty()) {
        // Responsive probes measure retained rich tags with unbounded width.
        // Refresh can run while positioning another window layer, after the
        // main tree was placed. Restore the real selector placement before
        // child Button/Input geometry is synchronized.
        const auto rect = services_->nodes().require(s->node).bounds;
        if (rect.width > 0 && rect.height > 0) {
            const auto selector = services_->components().root(s->selector);
            static_cast<void>(
                services_->layout().measure_child(selector, layout::Constraints::fixed(rect.width, rect.height)));
            services_->layout().place_child(selector, rect);
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
        }
    }
    s->tags_reconciling = false;
    if (geometry_changed) {
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
    }
    const bool again = s->tags_dirty;
    rollback.complete = true;
    if (again) {
        reconcile_tags(id);
    }
}

void SelectComponentHost::configure(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if ((s->mode == SelectMode::Single && (s->values_authority || s->labeled_values_authority)) ||
        (s->mode != SelectMode::Single && (s->authority || s->labeled_authority))) {
        throw std::invalid_argument("Select mode changes must preserve the typed controlled value model");
    }
    const auto revision = s->model_revision;
    const auto before = s->model;
    auto candidate = before;
    if (candidate.mode() != s->mode) {
        if (s->mode == SelectMode::Single) {
            candidate.reconcile(candidate.values().empty() ? SelectSelection{}
                                                           : SelectSelection{candidate.values().front()});
        } else if (candidate.mode() == SelectMode::Single) {
            candidate.reconcile_values(candidate.value() ? SelectValues{*candidate.value()} : SelectValues{});
        }
    }
    const auto filter = s->filter;
    const auto sort = s->sort;
    candidate.configure(
        s->mode,
        SelectFilter{[this, id, filter, field = s->filter_field](StringView query, const SelectOption& option) {
            if (!select_state(*services_, id)) {
                return false;
            }
            const bool result = filter.accepts(query, option, field);
            return select_state(*services_, id) && result;
        }},
        SelectSort{[this, id, sort](const SelectOption& a, const SelectOption& b, StringView query) {
            if (!select_state(*services_, id)) {
                return false;
            }
            const bool result = sort.before(a, b, query);
            return select_state(*services_, id) && result;
        }},
        s->active_first, s->hide_selected, s->max_count, s->filter_field, s->label_field);
    s = select_state(*services_, id);
    if (!s || s->model_revision != revision) {
        return;
    }
    s->model = std::move(candidate);
    ++s->model_revision;
    auto multiple = s->multiple;
    multiple.set(s->mode != SelectMode::Single);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (!s->show_search_explicit) {
        s->show_search = s->mode != SelectMode::Single;
    }
}

void SelectComponentHost::load(runtime::ComponentId id, String query) {
    auto* s = select_state(*services_, id);
    if (!s || !s->source || s->disabled || s->explicit_read_only || s->suppressed) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    const auto source = s->source;
    const auto async = s->async;
    s->pending = true;
    async->start(source, std::move(query), [this, id](AsyncOptionsResult<SelectOption> result) {
        auto* s = select_state(*services_, id);
        if (!s || s->suppressed || s->disabled || s->explicit_read_only || !window_active_ ||
            !services_->interactions().component_active(id)) {
            return;
        }
        SelectRetireOnFailure rollback{*services_, id};
        s->pending = false;
        if (!result.error.empty()) {
            const auto callback = s->on_load_error;
            if (callback) {
                callback(std::move(result.error));
            }
        } else {
            const auto changed = change_select_model(
                *services_, id, [&](auto& model) { return model.set_options(std::move(result.options)); });
            if (changed && *changed) {
                rebuild_rows(id);
            }
        }
        if (select_state(*services_, id)) {
            services_->request_frame();
        }
        rollback.complete = true;
    });
    rollback.complete = true;
}

void SelectComponentHost::mount_popup(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s) {
        return;
    }
    auto& build = runtime::require_component_build_context();
    s->popup_body = select_group(*services_);
    build.mount_slot(s->popup_body, Content{[this, id] {
                         auto* s = select_state(*services_, id);
                         if (!s) {
                             return;
                         }
                         const auto render = s->popup_renderer;
                         render([this, id] {
                             auto* s = select_state(*services_, id);
                             if (!s) {
                                 return;
                             }
                             if (s->list.valid()) {
                                 throw std::invalid_argument("Select popupRender must mount origin exactly once");
                             }
                             s->list = select_group(*services_, true);
                             const auto list = s->list;
                             services_->nodes().require(services_->components().root(list)).clip_content = true;
                             runtime::require_component_build_context().mount_slot(
                                 list, Content{[this, id] { mount_rows(id); }});
                         });
                         if (const auto* live = select_state(*services_, id); live && !live->list.valid()) {
                             throw std::invalid_argument("Select popupRender must mount origin exactly once");
                         }
                     }});
}

void SelectComponentHost::rebuild_popup(runtime::ComponentId id) {
    auto* s = select_state(*services_, id);
    if (!s || s->mounting) {
        return;
    }
    SelectRetireOnFailure rollback{*services_, id};
    const auto body = s->popup_body;
    const auto popup = s->popup;
    s->rows.clear();
    s->parked.clear();
    s->list = {};
    s->popup_body = {};
    services_->set_input_blocked(body, true);
    if (!select_state(*services_, id)) {
        return;
    }
    services_->destroy(body);
    if (!select_state(*services_, id)) {
        return;
    }
    services_->append_slot(popup, Content{[this, id] { mount_popup(id); }});
    refresh(id);
    rollback.complete = true;
}

void SelectComponentHost::mount(const SelectProps& props, const std::optional<SelectPrefix>& prefix,
                                const std::optional<SelectSuffix>& suffix) {
    auto& services = *services_;
    auto* inputs = services.input_runtime();
    if (!inputs) {
        throw std::logic_error("Select requires a window Input host");
    }
    if ((props.value_ && props.default_value_) || (props.open_ && props.default_open_)) {
        throw std::invalid_argument("Select cannot combine controlled and default values");
    }
    const auto height = read_prop(props.list_height_);
    validate_height(height);
    const auto mode = read_prop(props.mode_);
    if ((props.labeled_value_ && (props.value_ || props.default_value_ || props.values_ || props.labeled_values_ ||
                                  mode != SelectMode::Single)) ||
        (props.labeled_values_ &&
         (props.value_ || props.values_ || props.default_values_ || mode == SelectMode::Single))) {
        throw std::invalid_argument("Select labeled and raw value authorities cannot be mixed");
    }
    if ((props.values_ && props.default_values_) || (props.value_ && props.values_) ||
        (mode == SelectMode::Single && (props.values_ || props.default_values_)) ||
        (mode != SelectMode::Single && (props.value_ || props.default_value_))) {
        throw std::invalid_argument("Select single and multiple value models cannot be mixed");
    }
    std::vector<SelectOption> options = read_prop(props.options_);
    if (props.groups_) {
        options.clear();
        for (const auto& group : read_prop(*props.groups_)) {
            options.push_back({SelectValue{}, group.label, true, group.rich_label, true, group.key});
            options.insert(options.end(), group.options.begin(), group.options.end());
        }
    }
    SelectModel model{std::move(options), props.value_ ? read_prop(*props.value_) : props.default_value_,
                      props.value_.has_value() || props.values_.has_value() || props.labeled_value_.has_value() ||
                          props.labeled_values_.has_value()};
    model.configure(mode);
    if (props.labeled_value_) {
        const auto value = read_prop(*props.labeled_value_);
        model.reconcile(value ? SelectSelection{value->value} : SelectSelection{});
        model.reconcile_labels(value ? SelectLabeledValues{*value} : SelectLabeledValues{});
    }
    if (props.labeled_values_) {
        const auto labels = read_prop(*props.labeled_values_);
        SelectValues values;
        for (const auto& label : labels) {
            values.push_back(label.value);
        }
        model.reconcile_values(std::move(values));
        model.reconcile_labels(labels);
    }
    if (props.values_ || props.default_values_) {
        model.reconcile_values(props.values_ ? read_prop(*props.values_) : *props.default_values_);
    }
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<SelectState>(std::move(model));
    SelectRetireOnFailure rollback{services, id};
    auto& s = build.state<SelectState>(id);
    s.node = build.root(id);
    const auto bindings = s.bindings;
    build.on_resource_cleanup(id, [bindings] { bindings->dispose(); });
    build.on_resource_cleanup(id, [&services, id] { services.interactions().set_input_blocked(id, false); });
    s.reference = props.ref_.value_or(SelectRef{});
    s.mode = mode;
    s.multiple.set(mode != SelectMode::Single);
    s.labeled_authority = props.labeled_value_;
    s.labeled_values_authority = props.labeled_values_;
    s.authority = props.value_;
    s.values_authority = props.values_;
    s.search_authority = props.search_value_;
    s.separators = props.token_separators_;
    s.tokenizer = props.tokenizer_;
    for (const auto& separator : s.separators) {
        if (separator.empty()) {
            throw std::invalid_argument("Select token separators must be nonempty");
        }
    }
    s.on_values_change = props.on_values_change_;
    s.on_labeled_change = props.on_labeled_change_;
    s.on_select = props.on_select_;
    s.on_deselect = props.on_deselect_;
    s.on_active = props.on_active_;
    s.on_key = props.on_key_;
    s.on_clear = props.on_clear_;
    s.on_scroll = props.on_scroll_;
    s.on_load_error = props.on_load_error_;
    s.max_tag_placeholder = props.max_tag_placeholder_;
    s.option_renderer = read_prop(props.option_render_);
    s.label_renderer = read_prop(props.label_render_);
    s.tag_renderer = read_prop(props.tag_render_);
    s.popup_renderer = read_prop(props.popup_render_);
    const auto async = s.async;
    build.on_resource_cleanup(id, [async] { async->cancel(); });
    s.show_search = props.show_search_ ? read_prop(*props.show_search_) : mode != SelectMode::Single;
    s.show_search_explicit = props.show_search_.has_value();
    s.allow_clear = read_prop(props.allow_clear_);
    s.disabled = read_prop(props.disabled_);
    s.loading = read_prop(props.loading_);
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.controlled_open = props.open_.has_value();
    s.external_placeholder = read_prop(props.placeholder_);
    s.empty_label = read_prop(props.not_found_content_);
    s.list_height = height;
    s.on_change = props.on_change_;
    s.on_search = props.on_search_;
    s.on_open = props.on_open_change_;
    s.on_focus = props.on_focus_;
    s.on_blur = props.on_blur_;
    s.display.set(s.model.label());
    s.read_only.set(!s.show_search);
    runtime::connect_layout_style(*bindings, props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            Theme(ThemeProps{}.config(s.selector_theme), ThemeContent{[&] {
                      FormPrivateEditorScope private_editor;
                      const std::optional<InputPrefix> input_prefix = InputPrefix{[&] {
                          const auto prefix_root = select_group(services);
                          layout::FlexLayout prefix_layout;
                          prefix_layout.align = layout::FlexAlign::center;
                          prefix_layout.item_policy = layout::FlexItemPolicy::sequential;
                          prefix_layout.right_to_left = s.direction.get() == TextDirection::RightToLeft;
                          prefix_layout.main_gap = build.theme_scope()->snapshot().select().metrics.gap;
                          services.layout().set_layout(services.components().root(prefix_root), prefix_layout);
                          build.mount_slot(prefix_root, Content{[&] {
                                               if (prefix) {
                                                   SlotContentAccess::function (*prefix)();
                                               }
                                               s.selected_root = select_group(services);
                                               const auto node = services.components().root(s.selected_root);
                                               auto flex = prefix_layout;
                                               flex.item_policy = layout::FlexItemPolicy::flex;
                                               services.layout().set_layout(node, flex);
                                           }});
                      }};
                      const auto input_count = inputs->mounted_inputs().size();
                      Input(InputProps{}
                                .value(s.display)
                                .placeholder(s.placeholder)
                                .readOnly(s.read_only)
                                .disabled(props.disabled_)
                                .size(props.size_)
                                .variant(props.variant_)
                                .status(props.status_)
                                .direction(s.direction)
                                .ref(s.reference)
                                .autoFocus(props.auto_focus_)
                                .onChange([this, id](String query) { edit(id, std::move(query)); })
                                .onFocus([this, id] {
                                    auto* s = select_state(*services_, id);
                                    if (!s) {
                                        return;
                                    }
                                    s->focused = true;
                                    const auto callback = s->on_focus;
                                    if (callback) {
                                        invoke_select_callback(*services_, id, callback);
                                    }
                                })
                                .onBlur([this, id] {
                                    auto* s = select_state(*services_, id);
                                    if (!s || !services_->components().scope(id).active()) {
                                        return;
                                    }
                                    s->focused = false;
                                    const auto callback = s->on_blur;
                                    const auto focus = services_->focus().state().focused;
                                    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
                                    const bool internal = record && descendant(*services_, record->component, id);
                                    if (!internal) {
                                        s->suppressed = true;
                                        request_open(id, false);
                                    }
                                    if (services_->components().contains(id) && callback) {
                                        invoke_select_callback(*services_, id, callback);
                                    }
                                }),
                            input_prefix, InputSuffix{[&] {
                                auto& children = runtime::require_component_build_context();
                                s.clear_wrapper = children.mount_component<int>(0);
                                const auto clear_node = children.root(s.clear_wrapper);
                                services.layout().set_layout(clear_node, layout::BoxLayout{});
                                const auto fragment = children.register_scene_fragment(
                                    s.clear_wrapper, runtime::SceneFragmentPlacement::before_children);
                                graphics::QuadInstance initial;
                                initial.opacity = 0;
                                component::RetainedSurfaceEffects effects;
                                effects.focus_enabled = false;
                                s.clear_background = services.surfaces().create_surface(
                                    s.clear_wrapper, clear_node, fragment, std::span{&initial, 1}, effects);
                                children.on_resource_cleanup(s.clear_wrapper,
                                                             [&services, clear_node, surface = s.clear_background] {
                                                                 services.surfaces().destroy(surface);
                                                                 services.layout().remove_layout(clear_node);
                                                             });
                                children.mount_slot(s.clear_wrapper, Content{[&] {
                                                        mount_input_affix_action(
                                                            services, s.clear_icon, s.action_disabled,
                                                            [this, id] { choose(id, {}); }, s.clear_visible, false,
                                                            false);
                                                    }});
                                if (suffix) {
                                    SlotContentAccess::function (*suffix)();
                                } else {
                                    Icon(IconProps{}.source(s.arrow).spin(s.spinning).tone(TextTone::Secondary));
                                }
                            }});
                      const auto input = inputs->mounted_inputs()[input_count];
                      s.selector = input.component;
                      s.selector_hit = input.interaction;
                      s.editor = input.editor;
                      inputs->configure_form_reset(s.selector, [this, id] {
                          auto* s = select_state(*services_, id);
                          if (!s) {
                              return;
                          }
                          auto async = s->async;
                          s->pending = false;
                          async->cancel();
                          s = select_state(*services_, id);
                          if (!s) {
                              return;
                          }
                          if (s->authority) {
                              s->model.reconcile(read_prop(*s->authority));
                          }
                          if (s->values_authority) {
                              s->model.reconcile_values(read_prop(*s->values_authority));
                          }
                          s->scroll = 0;
                          request_open(id, false);
                          refresh(id);
                      });
                  }});
            s.popup = runtime::require_component_build_context().mount_component<int>(0);
            const auto popup_node = build.root(s.popup);
            services.layout().set_layout(popup_node, layout::LeafLayout{});
            services.components().set_window_layer(s.popup, build.theme_scope()->snapshot().select().z_index_popup);
            s.popup_hit = services.interactions().create({s.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.background =
                services.surfaces().create(s.popup, popup_node, fragment, s.popup_hit, std::span{&initial, 1}, effects);
            build.on_resource_cleanup(
                s.popup, [&services, popup = s.popup, popup_node, hit = s.popup_hit, surface = s.background] {
                    services.interactions().set_input_blocked(popup, false);
                    services.pointer().cancel_interaction(hit);
                    services.interactions().remove(hit);
                    services.surfaces().destroy(surface);
                    services.layout().remove_layout(popup_node);
                });
            build.mount_slot(s.popup, Content{[this, id] { mount_popup(id); }});
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints constraints) {
                        const auto* s = select_state(*services_, id);
                        return engine.measure_child(services_->components().root(s->selector), constraints);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                        const auto* s = select_state(*services_, id);
                        engine.place_child(services_->components().root(s->selector), bounds);
                    }});
    build.on_resource_cleanup(id, [&services, node = s.node] { services.layout().remove_layout(node); });
    mounted_.push_back(id);
    s.mounting = false;
    update_theme(id);
    auto& scope = *bindings;
    const auto connect = [&](const auto& prop, auto apply) {
        if (!select_state(services, id)) {
            return;
        }
        connect_prop(scope, prop, [&services, id, apply = std::move(apply)](auto value) {
            if (select_state(services, id)) {
                apply(std::move(value));
            }
        });
    };
    if (!props.groups_) {
        connect(props.options_, [this, id](std::vector<SelectOption> value) {
            const auto changed =
                change_select_model(*services_, id, [&](auto& model) { return model.set_options(std::move(value)); });
            if (changed && *changed) {
                rebuild_rows(id);
            }
        });
    }
    if (props.groups_) {
        connect(*props.groups_, [this, id](std::vector<SelectOptionGroup> groups) {
            std::vector<SelectOption> options;
            for (auto& group : groups) {
                options.push_back({SelectValue{}, group.label, true, group.rich_label, true, group.key});
                options.insert(options.end(), group.options.begin(), group.options.end());
            }
            const auto changed =
                change_select_model(*services_, id, [&](auto& model) { return model.set_options(std::move(options)); });
            if (changed && *changed) {
                rebuild_rows(id);
            }
        });
    }
    if (props.labeled_value_) {
        connect(*props.labeled_value_, [this, id](SelectLabeledSelection label) {
            const auto changed = change_select_model(*services_, id, [&](auto& model) {
                model.reconcile(label ? SelectSelection{label->value} : SelectSelection{});
                model.reconcile_labels(label ? SelectLabeledValues{*label} : SelectLabeledValues{});
                return true;
            });
            if (changed) {
                refresh(id);
            }
        });
    }
    if (props.labeled_values_) {
        connect(*props.labeled_values_, [this, id](SelectLabeledValues labels) {
            SelectValues values;
            for (const auto& label : labels) {
                values.push_back(label.value);
            }
            const auto changed = change_select_model(*services_, id, [&](auto& model) {
                model.reconcile_values(std::move(values));
                model.reconcile_labels(std::move(labels));
                return true;
            });
            if (changed) {
                refresh(id);
            }
        });
    }
    if (props.values_) {
        connect(*props.values_, [this, id](SelectValues values) {
            const auto changed = change_select_model(
                *services_, id, [&](auto& model) { return model.reconcile_values(std::move(values)); });
            if (changed && *changed) {
                refresh(id);
            }
        });
    }
    if (props.search_value_) {
        connect(*props.search_value_, [this, id](String query) {
            const auto changed =
                change_select_model(*services_, id, [&](auto& model) { return model.search(std::move(query)); });
            if (changed && *changed) {
                refresh(id);
            }
        });
    }
    if (props.value_) {
        connect(*props.value_, [this, id](SelectSelection value) {
            const auto changed =
                change_select_model(*services_, id, [&](auto& model) { return model.reconcile(std::move(value)); });
            if (changed && *changed) {
                refresh(id);
            }
        });
    }
    if (props.open_) {
        connect(*props.open_, [this, id](bool value) {
            if (auto* s = select_state(*services_, id)) {
                s->open = value;
                s->suppressed = false;
                if (!value) {
                    if (!change_select_model(*services_, id, [](auto& model) {
                            model.search({});
                            return true;
                        })) {
                        return;
                    }
                }
                refresh(id);
            }
        });
    }
    connect(props.placeholder_, [this, id](String value) {
        if (auto* s = select_state(*services_, id)) {
            s->external_placeholder = std::move(value);
            refresh(id, false);
        }
    });
    if (props.show_search_) {
        connect(*props.show_search_, [this, id](bool value) {
            if (auto* s = select_state(*services_, id)) {
                s->show_search = value;
                if (!change_select_model(*services_, id, [](auto& model) {
                        model.search({});
                        return true;
                    })) {
                    return;
                }
                refresh(id);
            }
        });
    }
    connect(props.allow_clear_, [this, id](bool value) {
        if (auto* s = select_state(*services_, id)) {
            s->allow_clear = value;
            refresh(id, false);
        }
    });
    connect(props.loading_, [this, id](bool value) {
        if (auto* s = select_state(*services_, id)) {
            s->loading = value;
            refresh(id, false);
        }
    });
    connect(props.disabled_, [this, id](bool value) {
        if (auto* s = select_state(*services_, id)) {
            s->disabled = value;
            if (value) {
                s->suppressed = true;
                request_open(id, false);
            }
            refresh(id);
        }
    });
    connect(props.not_found_content_, [this, id](String value) {
        if (auto* s = select_state(*services_, id)) {
            s->empty_label = std::move(value);
            refresh(id);
        }
    });
    connect(props.list_height_, [this, id](float value) {
        validate_height(value);
        if (auto* s = select_state(*services_, id)) {
            s->list_height = value;
            refresh(id);
        }
    });
    const auto mirror_icon = [&](const auto& prop, auto member) {
        connect(prop, [this, id, member](IconSource icon) {
            if (auto* s = select_state(*services_, id)) {
                auto signal = s->*member;
                signal.set(std::move(icon));
                refresh(id, false);
            }
        });
    };
    mirror_icon(props.clear_icon_, &SelectState::clear_icon);
    mirror_icon(props.suffix_icon_, &SelectState::suffix_icon);
    mirror_icon(props.search_icon_, &SelectState::search_icon);
    mirror_icon(props.loading_icon_, &SelectState::loading_icon);
    mirror_icon(props.remove_icon_, &SelectState::remove_icon);
    mirror_icon(props.selected_icon_, &SelectState::selected_icon);
    const auto simple = [&](const auto& prop, auto member, bool configure_model = false) {
        connect(prop, [this, id, member, configure_model](auto value) {
            if (auto* s = select_state(*services_, id)) {
                if (s->*member == value) {
                    return;
                }
                const auto previous = s->*member;
                s->*member = std::move(value);
                if (configure_model) {
                    try {
                        configure(id);
                    } catch (...) {
                        if (auto* live = select_state(*services_, id)) {
                            live->*member = previous;
                        }
                        throw;
                    }
                }
                refresh(id);
            }
        });
    };
    simple(props.mode_, &SelectState::mode, true);
    simple(props.filter_, &SelectState::filter, true);
    simple(props.sort_, &SelectState::sort, true);
    simple(props.filter_field_, &SelectState::filter_field, true);
    simple(props.label_field_, &SelectState::label_field, true);
    simple(props.active_first_, &SelectState::active_first, true);
    simple(props.hide_selected_, &SelectState::hide_selected, true);
    simple(props.max_count_, &SelectState::max_count, true);
    simple(props.auto_clear_search_, &SelectState::auto_clear_search);
    simple(props.max_tag_count_, &SelectState::max_tag_count);
    simple(props.max_tag_text_, &SelectState::max_tag_text);
    simple(props.responsive_tags_, &SelectState::responsive_tags);
    connect(props.read_only_prop_, [this, id](bool value) {
        if (auto* s = select_state(*services_, id)) {
            s->explicit_read_only = value;
            if (value) {
                s->suppressed = true;
                request_open(id, false);
            }
            refresh(id);
        }
    });
    simple(props.virtual_, &SelectState::virtual_list);
    simple(props.match_width_, &SelectState::match_width);
    simple(props.motion_, &SelectState::animate);
    connect(props.popup_width_, [this, id](float width) {
        if (!std::isfinite(width) || width < 0) {
            throw std::invalid_argument("Select popupWidth must be finite and nonnegative");
        }
        if (auto* s = select_state(*services_, id)) {
            s->popup_width = width;
            refresh(id);
        }
    });
    connect(props.direction_, [this, id](TextDirection direction) {
        if (direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
            direction != TextDirection::RightToLeft) {
            throw std::invalid_argument("Invalid Select direction");
        }
        if (auto* s = select_state(*services_, id)) {
            auto signal = s->direction;
            signal.set(direction);
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
            auto flex = layout::FlexLayout{};
            flex.right_to_left = direction == TextDirection::RightToLeft;
            flex.main_gap = services_->components().theme_scope(id)->snapshot().select().metrics.gap;
            flex.align = layout::FlexAlign::center;
            services_->layout().set_layout(services_->components().root(s->selected_root), flex);
            const auto prefix = services_->components().parent(s->selected_root);
            flex.item_policy = layout::FlexItemPolicy::sequential;
            services_->layout().set_layout(services_->components().root(*prefix), flex);
            refresh(id);
        }
    });
    connect(props.placement_, [this, id](SelectPlacement placement) {
        if (placement < SelectPlacement::BottomLeft || placement > SelectPlacement::TopRight) {
            throw std::invalid_argument("Invalid Select placement");
        }
        if (auto* s = select_state(*services_, id)) {
            s->placement = placement;
            refresh(id);
        }
    });
    connect(props.option_render_, [this, id](SelectOptionRenderer render) {
        if (auto* s = select_state(*services_, id); s && s->option_renderer != render) {
            s->option_renderer = std::move(render);
            rebuild_rows(id);
        }
    });
    simple(props.tag_render_, &SelectState::tag_renderer);
    simple(props.label_render_, &SelectState::label_renderer);
    connect(props.popup_render_, [this, id](SelectPopupRenderer render) {
        if (auto* s = select_state(*services_, id); s && s->popup_renderer != render) {
            s->popup_renderer = std::move(render);
            rebuild_popup(id);
        }
    });
    connect(props.source_, [this, id](SelectSource source) {
        if (auto* s = select_state(*services_, id); s && s->source != source) {
            s->source = std::move(source);
            auto async = s->async;
            s->pending = false;
            async->cancel();
        }
    });
    if (!select_state(services, id)) {
        rollback.complete = true;
        return;
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::select_colors, &theme_runtime::ThemeScope::select_metrics,
                               &theme_runtime::ThemeScope::select_typography,
                               &theme_runtime::ThemeScope::select_effects, &theme_runtime::ThemeScope::select_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        auto subscription = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i], selector_effects = i == 0] {
                static_cast<void>((theme.get()->*accessor)());
                if (selector_effects) {
                    static_cast<void>(theme->input_shadows());
                }
            });
        auto* live = select_state(services, id);
        if (!live) {
            rollback.complete = true;
            return;
        }
        live->subscriptions[i] = std::move(subscription);
    }
    refresh(id);
    rollback.complete = true;
}

void SelectComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = select_state(*services_, id); s && !active) {
            s->suppressed = true;
            request_open(id, false);
        }
        refresh(id);
    }
}

void SelectComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = select_state(*services_, id);
            s && s->open && !s->suppressed && !services_->interactions().component_active(id)) {
            s->suppressed = true;
            request_open(id, false);
        } else {
            refresh(id, false);
        }
    }
}

void SelectComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    const auto focus = services_->focus().state().focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    const auto focused_component = record ? std::optional{record->component} : std::nullopt;
    for (auto id : ids) {
        auto* s = select_state(*services_, id);
        if (!s) {
            continue;
        }
        const bool inside = focused_component && descendant(*services_, *focused_component, id);
        const bool exited = s->focus_was_inside && !inside;
        s->focus_was_inside = inside;
        if (exited && s->visible) {
            s->suppressed = true;
            request_open(id, false);
        }
    }
    // Retained branch hiding changes input activity without changing the Modal/Spin boundary revision.
    // Use the same suppression and close intent before layout can merely clear `visible`.
    const bool inactive_popup = std::ranges::any_of(mounted_, [this](auto id) {
        const auto* s = select_state(*services_, id);
        return s && s->open && !s->suppressed && !services_->interactions().component_active(id);
    });
    if (inactive_popup) {
        on_input_boundary_changed();
    }
}

bool SelectComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (event.action != input::KeyAction::down || !window_active_) {
        return false;
    }
    const auto focus = services_->focus().state().focused;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = select_state(*services_, id);
        if (!s || s->disabled || s->explicit_read_only) {
            continue;
        }
        const auto* focused_record = focus ? services_->interactions().find(*focus) : nullptr;
        if (!focused_record || !descendant(*services_, focused_record->component, id)) {
            continue;
        }
        if (focus != s->selector_hit) {
            auto current_layer = std::optional{focused_record->component};
            while (current_layer && !services_->components().is_window_layer_root(*current_layer)) {
                current_layer = services_->components().parent(*current_layer);
            }
            const auto session = services_->input_runtime()->sessions().active();
            const auto* editor = services_->input_runtime()->editors().find(session.owner);
            if (editor && editor->composition().active) {
                return false;
            }
            if (event.key == input::Key::escape && s->visible && current_layer == std::optional{s->popup} &&
                !services_->popover().owns_escape(id, focused_record->component)) {
                const auto target = s->selector_hit;
                request_open(id, false);
                if (select_state(*services_, id)) {
                    services_->focus().defer_focus(target, input::FocusModality::keyboard);
                }
                return true;
            }
            if (event.key == input::Key::tab && descendant(*services_, focused_record->component, s->popup)) {
                std::vector<input::InteractionId> order;
                for (auto hit : services_->interactions().declaration_order()) {
                    const auto* record = services_->interactions().find(hit);
                    if (record && record->eligible && record->focusable && record->tab_stop &&
                        services_->interactions().branch_active(hit) &&
                        !descendant(*services_, record->component, s->popup)) {
                        order.push_back(hit);
                    }
                }
                const auto anchor = std::ranges::find(order, s->selector_hit);
                const auto index = static_cast<std::size_t>(anchor - order.begin());
                const bool reverse = input::has_modifier(event.modifiers, input::KeyModifier::shift);
                const auto target =
                    anchor != order.end()
                        ? std::optional{order[(index + (reverse ? order.size() - 1 : 1)) % order.size()]}
                        : std::nullopt;
                request_open(id, false);
                if (target && select_state(*services_, id)) {
                    services_->focus().defer_focus(*target, input::FocusModality::keyboard);
                }
                return true;
            }
            continue;
        }
        const bool composing = services_->input_runtime()->editors().require(s->editor).composition().active;
        const auto key_callback = s->on_key;
        if (key_callback) {
            SelectInputKey key = SelectInputKey::Other;
            switch (event.key) {
            case input::Key::enter:
                key = SelectInputKey::Enter;
                break;
            case input::Key::escape:
                key = SelectInputKey::Escape;
                break;
            case input::Key::tab:
                key = SelectInputKey::Tab;
                break;
            case input::Key::up:
                key = SelectInputKey::Up;
                break;
            case input::Key::down:
                key = SelectInputKey::Down;
                break;
            case input::Key::home:
                key = SelectInputKey::Home;
                break;
            case input::Key::end:
                key = SelectInputKey::End;
                break;
            case input::Key::backspace:
                key = SelectInputKey::Backspace;
                break;
            case input::Key::space:
                key = SelectInputKey::Space;
                break;
            case input::Key::left:
                key = SelectInputKey::Left;
                break;
            case input::Key::right:
                key = SelectInputKey::Right;
                break;
            default:
                break;
            }
            SelectRetireOnFailure rollback{*services_, id};
            const bool handled = key_callback({key, event.repeat, composing,
                                               input::has_modifier(event.modifiers, input::KeyModifier::shift),
                                               input::has_modifier(event.modifiers, input::KeyModifier::control),
                                               input::has_modifier(event.modifiers, input::KeyModifier::alt),
                                               input::has_modifier(event.modifiers, input::KeyModifier::meta)});
            rollback.complete = true;
            s = select_state(*services_, id);
            if (!s || handled) {
                return true;
            }
        }
        if (composing) {
            return false;
        }
        using input::Key;
        if (event.key == Key::tab) {
            request_open(id, false);
            return false;
        }
        if (event.key == Key::escape && s->visible) {
            request_open(id, false);
            return true;
        }
        if (event.modifiers != input::KeyModifier::none) {
            return false;
        }
        if (s->mode != SelectMode::Single && event.key == Key::backspace && s->model.query().empty() && !event.repeat) {
            auto values = s->model.values();
            for (auto it = values.rbegin(); it != values.rend(); ++it) {
                if (!s->model.option(*it).disabled) {
                    values.erase(std::next(it).base());
                    publish_values(id, std::move(values));
                    break;
                }
            }
            return true;
        }
        if (s->mode == SelectMode::Tags && event.key == Key::enter && !s->model.query().empty()) {
            SelectRetireOnFailure rollback{*services_, id};
            if (!event.repeat) {
                if (s->visible && s->model.active() &&
                    s->model.options()[*s->model.active()].value != SelectValue{s->model.query()}) {
                    choose(id, *s->model.active());
                    rollback.complete = true;
                    return true;
                }

                auto candidate = s->model;
                const auto tokenize = s->tokenizer;
                const auto tokens =
                    tokenize ? tokenize(candidate.query().view()) : std::vector<String>{candidate.query()};
                if (!select_state(*services_, id)) {
                    return true;
                }
                const auto intent = candidate.request_tokens(tokens);
                if (intent && select_state(*services_, id)) {
                    publish_values(id, *intent);
                }
            }
            rollback.complete = true;
            return true;
        }
        if (event.key == Key::enter || (!s->show_search && event.key == Key::space)) {
            if (!event.repeat) {
                if (s->visible && s->model.active()) {
                    choose(id, *s->model.active());
                } else {
                    request_open(id, true);
                }
            }
            return true;
        }
        if (event.key != Key::up && event.key != Key::down && event.key != Key::home && event.key != Key::end) {
            return false;
        }
        if (!s->visible && (event.key == Key::home || event.key == Key::end)) {
            return false;
        }
        const bool was_open = s->visible;
        request_open(id, true);
        s = select_state(*services_, id);
        if (!s) {
            return true;
        }
        if (event.key == Key::home || event.key == Key::end) {
            s->model.boundary(event.key == Key::end);
        } else if (was_open || !s->model.active()) {
            s->model.move(event.key == Key::up ? -1 : 1);
        }
        const auto callback = s->on_active;
        const auto active_value =
            s->model.active() ? std::optional{s->model.options()[*s->model.active()].value} : std::nullopt;
        refresh(id);
        if (select_state(*services_, id) && callback && active_value) {
            invoke_select_callback(*services_, id, callback, *active_value);
        }
        return true;
    }
    return false;
}

void SelectComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                           std::optional<input::InteractionId> hit,
                                           std::optional<input::InteractionId> origin) {
    if (event.action != input::PointerAction::down && event.action != input::PointerAction::up) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = select_state(*services_, id);
        if (!s || s->disabled) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_control = record && descendant(*services_, record->component, id);
        if (event.action == input::PointerAction::down && !in_control) {
            if (s->visible || (s->open && !s->suppressed)) {
                request_open(id, false);
            }
            continue;
        }
        if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
            hit == s->selector_hit && origin == hit) {
            request_open(id, s->show_search || !s->visible);
        }
    }
}

bool SelectComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (mounted_.empty()) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return false;
    }
    // The closest layer owns the wheel, independent of popup bounds or mount order.
    auto layer = std::optional{target->component};
    while (layer && !services_->components().is_window_layer_root(*layer)) {
        layer = services_->components().parent(*layer);
    }
    if (!layer) {
        return false;
    }
    for (auto it = mounted_.rbegin(); it != mounted_.rend(); ++it) {
        auto* s = services_->components().state<SelectState>(*it);
        if (!s || !s->visible || s->popup != *layer || !services_->interactions().component_active(s->popup) ||
            !inside(services_->nodes().require(services_->components().root(s->list)).bounds, event.x, event.y)) {
            continue;
        }
        const auto& token = services_->components().theme_scope(*it)->snapshot().select();
        const float height = token.metrics.option_height;
        const float content = static_cast<float>(std::max<std::size_t>(1, s->model.filtered().size())) * height;
        const auto menu = intersect(services_->nodes().require(services_->components().root(s->list)).bounds,
                                    services_->nodes().require(services_->components().root(s->popup_body)).bounds);
        const float available = menu.height;
        const float next =
            std::clamp(s->scroll - event.delta_y * height * 3, 0.0F, std::max(0.0F, content - available));
        if (next != s->scroll) {
            s->scroll = next;
            s->reveal_active = false;
            s->needs_layout = true;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            const auto callback = s->on_scroll;
            if (callback) {
                invoke_select_callback(*services_, *it, callback, next);
            }
        }
        return true;
    }
    return false;
}

void SelectComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, clip);
    }
}

void SelectComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport, runtime::Rect clip) {
    if (std::ranges::find(positioning_, id) != positioning_.end()) {
        return;
    }
    positioning_.push_back(id);

    struct PositionGuard final {
        std::vector<runtime::ComponentId>& stack;

        ~PositionGuard() {
            stack.pop_back();
        }
    } guard{positioning_};

    services_->popover().position_ancestors(id, viewport, clip);
    auto* s = select_state(*services_, id);
    if (!s) {
        return;
    }
    if (!services_->interactions().component_active(id)) {
        if (s->visible) {
            refresh(id, false);
        }
        return;
    }
    const auto& placed = services_->nodes().require(s->node);
    if (placed.place_count != 0) {
        const bool zero = placed.bounds.width <= 0 || placed.bounds.height <= 0;
        if (s->zero_geometry != zero) {
            s->zero_geometry = zero;
            if (zero) {
                s->suppressed = true;
                request_open(id, false);
            }
            if (!select_state(*services_, id)) {
                return;
            }
            services_->set_input_blocked(id, zero);
            s = select_state(*services_, id);
            if (!s) {
                return;
            }
        }
    }
    const auto control_bounds = services_->nodes().require(s->node).bounds;
    const runtime::Size tag_size{control_bounds.width, control_bounds.height};
    if (s->responsive_tags && s->tag_size != tag_size) {
        s->tag_size = tag_size;
        reconcile_tags(id);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        const auto selector = services_->components().root(s->selector);
        (void)services_->layout().measure_child(
            selector, layout::Constraints::fixed(control_bounds.width, control_bounds.height));
        services_->layout().place_child(selector, control_bounds);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
    }
    if (!s->visible) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().select();
    const auto& metrics = token.metrics;
    auto anchor = services_->nodes().require(s->node).bounds;
    const auto translation = services_->nodes().require(s->node).translation;
    anchor.x += translation.x;
    anchor.y += translation.y;
    const auto popup_node = services_->components().root(s->popup);
    if (!s->needs_layout && s->layout_anchor == anchor && s->layout_clip == clip &&
        s->layout_generation == services_->layout().generation()) {
        return;
    }
    const std::vector<std::size_t> filtered{s->model.filtered().begin(), s->model.filtered().end()};
    const float content = static_cast<float>(std::max<std::size_t>(1, filtered.size())) * metrics.option_height;
    const float below = std::clamp(clip.y + clip.height - anchor.y - anchor.height - metrics.gap, 0.0F, clip.height);
    const float above = std::clamp(anchor.y - metrics.gap - clip.y, 0.0F, clip.height);
    const float width = std::min(std::max(anchor.width, s->popup_width), clip.width);
    const float inset = std::min(metrics.popup_padding, width / 2);
    auto& engine = services_->layout();
    const auto body_node = services_->components().root(s->popup_body);
    const auto list_node = services_->components().root(s->list);
    engine.set_layout(list_node,
                      layout::LeafLayout{{std::max(0.0F, width - inset * 2), std::min(s->list_height, content)}});
    const auto body_size =
        engine.measure_child(body_node, {std::max(0.0F, width - inset * 2), std::max(0.0F, width - inset * 2), 0,
                                         std::max(0.0F, std::max(below, above) - inset * 2)});
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    const float wanted = body_size.height + metrics.popup_padding * 2;
    const bool prefer_above = s->placement == SelectPlacement::TopLeft || s->placement == SelectPlacement::TopRight;
    s->above = prefer_above ? (above >= wanted || above >= below) : (below < wanted && above > below);
    const bool align_right = s->placement == SelectPlacement::BottomRight ||
                             s->placement == SelectPlacement::TopRight ||
                             s->direction.get() == TextDirection::RightToLeft;
    const float height = std::min(wanted, s->above ? above : below);
    s->popup_bounds = {
        std::clamp(align_right ? anchor.x + anchor.width - width : anchor.x, clip.x, clip.x + clip.width - width),
        std::clamp(s->above ? anchor.y - metrics.gap - height : anchor.y + anchor.height + metrics.gap, clip.y,
                   clip.y + clip.height - height),
        width, height};
    (void)engine.measure_child(popup_node, layout::Constraints::fixed(width, height));
    engine.place_child(popup_node, s->popup_bounds);
    const runtime::Rect body_bounds{s->popup_bounds.x + inset, s->popup_bounds.y + inset,
                                    std::max(0.0F, width - inset * 2), std::max(0.0F, height - inset * 2)};
    (void)engine.measure_child(body_node, layout::Constraints::fixed(body_bounds.width, body_bounds.height));
    engine.place_child(body_node, body_bounds);
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    const auto menu = intersect(services_->nodes().require(list_node).bounds, body_bounds);
    s->scroll = std::clamp(s->scroll, 0.0F, std::max(0.0F, content - menu.height));
    if (s->reveal_active && s->model.active()) {
        const auto it = std::ranges::find(filtered, *s->model.active());
        if (it != filtered.end()) {
            const float top = static_cast<float>(it - filtered.begin()) * metrics.option_height;
            s->scroll = menu.height < metrics.option_height
                            ? top
                            : std::clamp(s->scroll, std::max(0.0F, top + metrics.option_height - menu.height), top);
        }
    }
    s->reveal_active = false;
    if (s->model.options().size() > 32 && s->virtual_list && s->match_width) {
        rebuild_rows(id);
        s = select_state(*services_, id);
        if (!s) {
            return;
        }
        s->reveal_active = false;
    }
    const auto rows = s->rows;
    const auto scroll = s->scroll;
    for (auto child : rows) {
        const auto* row = services_->components().state<SelectRow>(child);
        if (!row) {
            continue;
        }
        const auto it = row->index ? std::ranges::find(filtered, *row->index) : filtered.end();
        if (it == filtered.end() && !(filtered.empty() && !row->index)) {
            continue;
        }
        const float top = it == filtered.end() ? 0 : static_cast<float>(it - filtered.begin()) * metrics.option_height;
        const float padding = std::min(metrics.option_padding_inline, menu.width / 2);
        const auto node = row->node;
        engine.set_layout(
            node, layout::BoxLayout{{padding, metrics.option_padding_block, padding, metrics.option_padding_block}});
        (void)engine.measure_child(node, layout::Constraints::fixed(menu.width, metrics.option_height));
        engine.place_child(node, {menu.x, menu.y + top - scroll, menu.width, metrics.option_height});
        if (!select_state(*services_, id)) {
            return;
        }
        const auto visible = intersect(services_->nodes().require(node).bounds, menu);
        const bool blocked = visible.height <= 0 || visible.width <= 0 || row->disabled.get();
        if (blocked) {
            restore_row_focus(*services_, id, child, s->selector_hit);
            if (!select_state(*services_, id)) {
                return;
            }
        }
        services_->set_input_blocked(child, blocked);
        if (!select_state(*services_, id)) {
            return;
        }
    }
    s = select_state(*services_, id);
    if (!s) {
        return;
    }
    s->needs_layout = false;
    s->layout_anchor = anchor;
    s->layout_clip = clip;
    s->layout_generation = engine.generation();
}

void SelectComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect window{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = select_state(*services_, id);
        if (!s) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().select();
        const auto clear_bounds = services_->nodes().require(services_->components().root(s->clear_wrapper)).bounds;
        auto clear = quad(clear_bounds, token.colors.clear_background);
        clear.opacity = s->clear_visible.get() ? 1.0F : 0.0F;
        static_cast<void>(services_->surfaces().update_surface(s->clear_background, std::span{&clear, 1}));
        if (!s->visible) {
            continue;
        }
        services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
        const auto bounds = s->popup_bounds;
        auto background = quad(bounds, token.colors.popup_background, token.metrics.border_radius);
        background.opacity = s->motion.opacity;
        static_cast<void>(services_->surfaces().update_surface(s->background, std::span{&background, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, token.metrics.border_radius};
        effects.shadows = token.shadow;
        effects.shadow_opacity = s->motion.opacity;
        effects.ancestor_clip = graphics::EffectClip{1, window};
        static_cast<void>(services_->surfaces().update_effects(s->background, effects));
        const float padding = std::min(token.metrics.popup_padding, std::min(bounds.width, bounds.height) / 2);
        const runtime::Rect body_clip{bounds.x + padding, bounds.y + padding,
                                      std::max(0.0F, bounds.width - padding * 2),
                                      std::max(0.0F, bounds.height - padding * 2)};
        const auto menu_clip =
            intersect(services_->nodes().require(services_->components().root(s->list)).bounds, body_clip);
        for (auto row_id : s->rows) {
            const auto* row = services_->components().state<SelectRow>(row_id);
            if (!row || !services_->components().branch_active(row_id)) {
                continue;
            }
            const bool selected = row->index && s->model.selected(s->model.options()[*row->index].value);
            const bool active = row->index && s->model.active() == row->index;
            auto item = quad(intersect(services_->nodes().require(row->node).bounds, menu_clip),
                             selected ? token.colors.option_selected_background : token.colors.option_active_background,
                             token.metrics.border_radius);
            item.opacity = selected || (active && !s->loading) ? s->motion.opacity : 0.0F;
            static_cast<void>(services_->surfaces().update_surface(row->surface, std::span{&item, 1}));
            services_->scene_composer().set_fragment(row->hit, {}, row->interaction, menu_clip);
        }
    }
}

std::size_t SelectComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed = 0;
    for (auto id : std::vector{mounted_}) {
        auto* s = select_state(*services_, id);
        if (!s) {
            continue;
        }
        const auto previous = s->motion.opacity;
        if (!s->visible || !s->animate || services_->motion_preference() == animation::MotionPreference::reduced ||
            !animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot()).enabled()) {
            s->motion.stop();
        }
        if (s->motion.tick(time) || previous != s->motion.opacity) {
            services_->nodes().require(services_->components().root(s->popup)).content_opacity = s->motion.opacity;
            ++changed;
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> SelectComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> next;
    for (auto id : mounted_) {
        if (const auto* s = select_state(*services_, id)) {
            const auto deadline = s->motion.deadline();
            if (deadline && (!next || *deadline < *next)) {
                next = deadline;
            }
        }
    }
    return next;
}
} // namespace ryn::detail

namespace ryn {
void SelectPopupRenderer::operator()(std::function<void()> origin) const {
    if (function_) {
        (*function_)(std::move(origin));
    } else {
        origin();
    }
}

void Select(SelectProps props, std::optional<SelectPrefix> prefix, std::optional<SelectSuffix> suffix) {
    if (!detail::active_selects) {
        throw std::logic_error("Select requires window component services");
    }
    const auto attach = detail::FormAutoControlAccess::bind(props);
    detail::active_selects->mount(props, prefix, suffix);
    if (attach) {
        attach();
    }
}
} // namespace ryn
