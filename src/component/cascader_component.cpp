#include "component/cascader_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/input_component.hpp"
#include "component/popover_component.hpp"
#include "input/text_boundary.hpp"
#include "reactive/untracked.hpp"
#include <limits>
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/flex.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local CascaderComponentHost* active_cascader{};

struct Row final {
    CascaderOption option;
    CascaderPath path;
    std::vector<CascaderOption> ancestry;
    runtime::ComponentId component;
    runtime::ComponentId viewport;
    runtime::ComponentId rich_root;
    runtime::ComponentId label_root;
    Signal<TypographyContent> rich_label{TypographyContent{}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<IconSource> icon{IconSource{IconName::RightOutlined}};
    Signal<bool> icon_visible{false};
    Signal<FlexDirection> flow{FlexDirection::LeftToRight};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<IconTwoToneColor> icon_color{IconTwoToneColor{}};
    Signal<IconSource> check_icon{IconSource{IconName::BorderOutlined}};
    Signal<bool> check_visible{false};
    runtime::Rect viewport_bounds;
    float row_y{};
    float row_height{};
    Signal<String> label{String{}};
    Signal<bool> disabled{false};
    Signal<ButtonType> type{ButtonType::Text};
    ButtonRef ref;
    std::size_t column{};
    std::size_t order{};
    bool shown{};
    bool checked{};
    bool blocked{};
};

struct Tag final {
    CascaderPath path;
    String key;
    runtime::ComponentId component;
    runtime::ComponentId remove;
    runtime::ComponentId overflow_item;
    Signal<String> label{String{}};
    Signal<bool> disabled{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    runtime::Size size;
    bool shown{};
};

struct State final {
    CascaderProps props;
    Scope bindings;
    runtime::NodeId node;
    runtime::ComponentId panel;
    runtime::ComponentId content_root;
    runtime::ComponentId selector;
    runtime::ComponentId clear;
    runtime::ComponentId arrow;
    runtime::ComponentId tag_spacer;
    float tag_inline_width{};
    Signal<IconSource> arrow_source{IconSource{IconName::DownOutlined}};
    runtime::ComponentId search;
    runtime::ComponentId empty;
    runtime::ComponentId status;
    runtime::ComponentId retry_button;
    Signal<bool> retry_disabled{true};
    std::optional<String> retry_key;
    float status_height{};
    runtime::Size empty_size;
    std::vector<std::shared_ptr<Row>> rows;
    std::vector<std::shared_ptr<Tag>> tags;
    std::vector<CascaderOption> options;
    runtime::ComponentId tag_root;
    runtime::ComponentId overflow;
    runtime::ComponentId overflow_content;
    runtime::ComponentId footer;
    component::RetainedSurfaceId background;
    Signal<ThemeConfig> empty_theme{ThemeConfig{}};
    Signal<ThemeConfig> overflow_theme{ThemeConfig{}};
    Signal<IconTwoToneColor> arrow_color{IconTwoToneColor{}};
    Signal<IconTwoToneColor> clear_color{IconTwoToneColor{}};
    runtime::ComponentId suffix;
    runtime::ComponentId selector_wrapper;
    Signal<String> placeholder{String{}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<String> query_signal{String{}};
    Signal<FlexDirection> flow{FlexDirection::LeftToRight};
    Signal<PopoverPlacement> placement{PopoverPlacement::BottomLeft};
    Signal<String> overflow_label{String{}};
    Signal<bool> overflow_disabled{true};
    std::array<theme_runtime::Subscription, 4> subscriptions;
    float height{};
    float column_width{};
    float header_height{};
    float footer_height{};
    float viewport_height{};
    float horizontal_scroll{};
    bool reveal_columns{true};
    float available_tag_width{};
    float tag_height{};
    runtime::Rect panel_bounds;
    std::vector<float> extents;
    std::optional<String> reveal;
    bool geometry_available{true};
    bool focus_inside{};
    bool selector_focused{};
    bool wheel_scrolling{};
    bool reconciling{};
    bool measuring_tags{};
    std::optional<std::size_t> fitted_tags;
    float fitted_width{-1};
    std::vector<float> fitted_prefixes;
    std::uint64_t revision{};
    CascaderPaths value;
    CascaderPath expanded;
    Signal<String> display{String{}};
    Signal<String> selector_text{String{}};
    Signal<bool> selector_read_only{true};
    Signal<String> empty_label{String{u8"No data"}};
    Signal<bool> popup_open{false};
    Signal<bool> disabled{false};
    Signal<bool> clear_disabled{true};
    String query;
    String effective_query;
    String error;
    std::vector<float> scroll;
    std::shared_ptr<std::atomic_bool> cancelled{std::make_shared<std::atomic_bool>(false)};
    std::uint64_t generation{};
    std::optional<String> loading;
    std::thread::id thread{std::this_thread::get_id()};
    bool open{};
    bool embedded{};
    bool mounting{true};
    bool ready{};
    bool refreshing{};
};

using S = std::shared_ptr<State>;

S state(WindowComponentServices& v, runtime::ComponentId id) {
    const auto* s = v.components().state<S>(id);
    return s ? *s : S{};
}

template <class F> void invoke(WindowComponentServices& v, runtime::ComponentId id, F&& f) {
    try {
        f();
    } catch (...) {
        v.retire_failed_component(id);
        throw;
    }
}

void cancel_load(const S& s) {
    s->cancelled->store(true);
    ++s->generation;
    s->loading.reset();
}

bool contains(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

TypographyContent highlighted(String label, const String& query) {
    if (query.empty()) {
        return TypographyContent{{TypographyRun{label}}};
    }
    const auto keyword = input::text_search_key(query.bytes());
    input::TextBoundaryMap map;
    (void)map.assign(label.bytes());
    const auto bytes = map.grapheme_bytes();
    std::vector<bool> matches(map.grapheme_count());
    // Map normalized clusters back to original graphemes; folding may expand one scalar (ß → ss).
    std::string folded;
    std::vector<std::size_t> owners;
    for (std::size_t i = 0; i < map.grapheme_count(); ++i) {
        const auto part = input::text_search_key(label.bytes().substr(bytes[i], bytes[i + 1] - bytes[i]));
        folded += part;
        owners.insert(owners.end(), part.size(), i);
    }
    if (!keyword.empty()) {
        for (std::size_t p = 0; (p = folded.find(keyword, p)) != std::string::npos; p += keyword.size()) {
            for (std::size_t j = p; j < p + keyword.size(); ++j) {
                matches[owners[j]] = true;
            }
        }
    }
    std::vector<TypographyRun> runs;
    for (std::size_t i = 0; i < matches.size();) {
        auto end = i + 1;
        while (end < matches.size() && matches[end] == matches[i]) {
            ++end;
        }
        runs.push_back({String::from_utf8(label.bytes().substr(bytes[i], bytes[end] - bytes[i])).value(),
                        matches[i] ? std::optional{TypographyType::Success} : std::nullopt});
        i = end;
    }
    return TypographyContent{std::move(runs)};
}

String raw_label(const CascaderPath& path) {
    std::string out;
    for (const auto& v : path) {
        if (!out.empty()) {
            out += " / ";
        }
        out += std::holds_alternative<String>(v) ? std::string(std::get<String>(v).bytes())
                                                 : std::to_string(std::get<double>(v));
    }
    return String::from_utf8(out).value();
}

float large_font_size(float base) {
    return std::max(base, std::floor(base * std::exp(.2F) / 2.F) * 2.F);
}

ThemeConfig child_theme(const CascaderThemeToken& t, ControlSize size) {
    ThemeConfig c;
    c.button.tokens.border_radius = dp(t.metrics.radius);
    c.button.algorithm = true;
    c.button.seed.control_height = dp(size == ControlSize::Small   ? t.metrics.selector_height_small
                                      : size == ControlSize::Large ? t.metrics.selector_height_large
                                                                   : t.metrics.selector_height);
    const float button_font =
        size == ControlSize::Large ? large_font_size(t.typography.font_size) : t.typography.font_size;
    c.button.seed.font_size = dp(button_font);
    c.button.tokens.content_line_height = dp(t.typography.line_height + button_font - t.typography.font_size);
    c.button.tokens.default_background = t.colors.selector_background;
    c.button.tokens.default_color = t.colors.selector_color;
    c.button.tokens.default_border_color = t.colors.border;
    c.button.tokens.link_color = t.colors.active_border;
    c.button.tokens.link_hover_color = t.colors.hover_border;
    c.button.tokens.link_active_color = t.colors.active_border;
    c.button.tokens.text_background = Color(0, 0, 0, 0);
    c.button.tokens.text_color = t.colors.clear;
    c.button.tokens.text_hover_color = t.colors.clear;
    c.button.tokens.text_active_color = t.colors.clear;
    c.button.tokens.text_hover_background = t.colors.option_hover_background;
    c.button.tokens.text_active_background = t.colors.option_selected_background;
    c.input.tokens.color = t.colors.selector_color;
    c.input.tokens.placeholder_color = t.colors.placeholder;
    c.input.tokens.background = t.colors.selector_background;
    c.input.tokens.border_color = t.colors.border;
    c.input.tokens.hover_border_color = t.colors.hover_border;
    c.input.tokens.active_border_color = t.colors.active_border;
    c.input.tokens.input_line_height = dp(t.typography.line_height);
    c.input.tokens.input_line_height_small = dp(t.typography.line_height);
    c.input.tokens.input_line_height_large =
        dp(t.typography.line_height + large_font_size(t.typography.font_size) - t.typography.font_size);
    c.input.tokens.input_font_size = dp(t.typography.font_size);
    c.input.tokens.input_font_size_small = dp(t.typography.font_size);
    c.input.tokens.input_font_size_large = dp(large_font_size(t.typography.font_size));
    c.input.tokens.padding_inline = dp(t.metrics.selector_padding);
    c.input.tokens.border_radius = dp(t.metrics.radius);
    c.input.tokens.focus_width = dp(t.effects.focus_width);
    c.alias.line_width_focus = dp(t.effects.focus_width);
    c.alias.focus_outline_offset = dp(t.effects.focus_offset);
    c.input.tokens.control_height = dp(t.metrics.selector_height);
    c.input.tokens.control_height_small = dp(t.metrics.selector_height_small);
    c.input.tokens.control_height_large = dp(t.metrics.selector_height_large);
    c.alias.color_text_disabled = t.colors.disabled_color;
    c.input.tokens.disabled_background = t.colors.disabled_background;
    c.button.tokens.disabled_background = t.colors.disabled_background;
    c.alias.color_focus_outline = t.colors.focus;

    c.text.tokens.font_size = dp(t.typography.font_size);
    c.text.tokens.line_height = dp(t.typography.line_height);
    c.text.tokens.font_family = t.typography.font_family;
    c.text.tokens.font_weight = t.typography.font_weight;
    c.text.tokens.color = t.colors.option_color;
    c.popover.tokens.background = t.colors.popup_background;
    c.popover.tokens.padding = dp(0);
    c.popover.tokens.max_width = dp(std::max(10000.F, t.metrics.column_width * 33));
    c.popover.tokens.border_radius = dp(t.metrics.radius);
    c.popover.tokens.shadow = t.shadow;
    c.popover.tokens.z_index_popup = t.z_index_popup;
    return c;
}

ThemeConfig item_theme(const CascaderThemeToken& t, bool selected, bool tag = false, bool compact = false) {
    ThemeConfig c;
    const auto background = tag        ? t.colors.tag_background
                            : selected ? t.colors.option_selected_background
                                       : t.colors.popup_background;
    const auto color = tag ? t.colors.tag_color : selected ? t.colors.option_selected_color : t.colors.option_color;
    c.button.seed.font_size = dp(t.typography.font_size);
    c.button.tokens.content_line_height = dp(t.typography.line_height);
    c.button.tokens.content_line_height_small = dp(t.typography.line_height);
    c.button.tokens.content_line_height_large = dp(t.typography.line_height);
    c.button.algorithm = true;
    c.button.seed.control_height = dp((tag ? t.metrics.tag_height : t.metrics.option_height) + (compact ? 4.F : 0.F));

    c.button.tokens.default_background = background;
    c.button.tokens.text_background = background;
    c.button.tokens.text_color = color;
    c.button.tokens.text_hover_color = color;
    c.button.tokens.text_active_color = color;
    c.button.tokens.text_hover_background = t.colors.option_hover_background;
    c.button.tokens.text_active_background = t.colors.option_selected_background;
    c.button.tokens.default_color = color;
    c.button.tokens.border_width = dp(0);
    c.button.tokens.border_radius = dp(t.metrics.radius);
    c.button.tokens.default_shadow = ShadowList{};
    c.button.tokens.padding_inline = dp(tag ? t.metrics.tag_padding : t.metrics.option_padding_inline);
    c.alias.color_focus_outline = t.colors.focus;
    c.alias.line_width_focus = dp(t.effects.focus_width);
    c.alias.focus_outline_offset = dp(t.effects.focus_offset);
    c.alias.color_text_disabled = t.colors.disabled_color;
    c.input.tokens.disabled_background = t.colors.disabled_background;
    c.button.tokens.disabled_background = t.colors.disabled_background;
    c.text.tokens.color = color;
    c.text.tokens.font_size = dp(t.typography.font_size);
    c.text.tokens.line_height = dp(t.typography.line_height);
    c.text.tokens.font_weight = selected ? t.typography.selected_font_weight : t.typography.font_weight;
    c.text.tokens.font_family = t.typography.font_family;
    c.typography.tokens.text = color;
    c.typography.tokens.success = t.colors.search_highlight;
    c.typography.tokens.font_family = t.typography.font_family;
    c.typography.tokens.base_font_size = dp(t.typography.font_size);
    c.typography.tokens.base_line_height = dp(t.typography.line_height);
    c.typography.tokens.font_weight = selected ? t.typography.selected_font_weight : t.typography.font_weight;
    c.button.seed.color_primary = t.colors.option_hover_background;
    return c;
}

CascaderThemeToken sized_tags(CascaderThemeToken token, ControlSize size) {
    const float height = size == ControlSize::Small   ? token.metrics.selector_height_small
                         : size == ControlSize::Large ? token.metrics.selector_height_large
                                                      : token.metrics.selector_height;
    token.metrics.tag_height = std::max(.5F, token.metrics.tag_height + height - token.metrics.selector_height);
    if (size == ControlSize::Large) {
        const float font = large_font_size(token.typography.font_size);
        token.typography.line_height += font - token.typography.font_size;
        token.typography.font_size = font;
    }
    token.typography.line_height = std::min(token.typography.line_height, std::max(.5F, token.metrics.tag_height));
    return token;
}

bool prefix(const CascaderPath& a, const CascaderPath& b) {
    return a.size() <= b.size() && std::equal(a.begin(), a.end(), b.begin());
}

bool descendant(WindowComponentServices& v, runtime::ComponentId a, runtime::ComponentId b) {
    for (auto p = std::optional{a}; p && v.components().contains(*p); p = v.components().parent(*p)) {
        if (*p == b) {
            return true;
        }
    }
    return false;
}

void validate(const std::vector<CascaderOption>& list, std::vector<String>& keys, std::size_t depth = 0) {
    if (depth > 32) {
        throw std::invalid_argument("Cascader nesting exceeds 32");
    }
    std::vector<CascaderValue> values;
    for (const auto& x : list) {
        if (x.key.empty() || std::ranges::find(keys, x.key) != keys.end() ||
            std::ranges::find(values, x.value) != values.end() ||
            (std::holds_alternative<double>(x.value) && !std::isfinite(std::get<double>(x.value)))) {
            throw std::invalid_argument("Cascader requires unique keys, sibling values and finite numbers");
        }
        keys.push_back(x.key);
        values.push_back(x.value);
        validate(x.children, keys, depth + 1);
    }
}

void validate_paths(const CascaderPaths& paths, bool multiple) {
    if (!multiple && paths.size() > 1) {
        throw std::invalid_argument("single Cascader accepts at most one path");
    }
    CascaderPaths seen;
    for (const auto& path : paths) {
        if (path.empty() || std::ranges::find(seen, path) != seen.end()) {
            throw std::invalid_argument("Cascader paths must be nonempty and unique");
        }
        for (const auto& v : path) {
            if (std::holds_alternative<double>(v) && !std::isfinite(std::get<double>(v))) {
                throw std::invalid_argument("Cascader path numbers must be finite");
            }
        }
        seen.push_back(path);
    }
}

String join(const std::vector<CascaderOption>& path) {
    std::string text;
    for (const auto& x : path) {
        if (!text.empty()) {
            text += " / ";
        }
        text += (x.label.empty() ? raw_label({x.value}) : x.label).bytes();
    }
    return String::from_utf8(text).value();
}
} // namespace

CascaderComponentHost::CascaderComponentHost(WindowComponentServices& v) : services_(&v) {
    v.attach(*this);
}

CascaderComponentHost::~CascaderComponentHost() {
    services_->detach(*this);
}

void* CascaderComponentHost::begin_mount() noexcept {
    auto* old = active_cascader;
    active_cascader = this;
    return old;
}

void CascaderComponentHost::end_mount(void* p) noexcept {
    active_cascader = static_cast<CascaderComponentHost*>(p);
}

void CascaderComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
    prepare_auxiliary_layout();
}

void CascaderComponentHost::prepare_auxiliary_layout() {
    for (auto id : std::vector(mounted_)) {
        auto s = state(*services_, id);
        if (!s || s->mounting || s->reconciling || s->refreshing) {
            continue;
        }
        const auto exists = [&](runtime::ComponentId x) {
            return services_->components().contains(x);
        };
        if (!exists(s->panel) || !exists(s->search) || !exists(s->empty) || !exists(s->status) ||
            !exists(s->retry_button) || (!s->embedded && (!exists(s->selector) || !exists(s->tag_root))) ||
            std::ranges::any_of(s->rows,
                                [&](const auto& r) { return !exists(r->viewport) || !exists(r->component); }) ||
            std::ranges::any_of(s->tags,
                                [&](const auto& t) { return !exists(t->component) || !exists(t->overflow_item); })) {
            services_->retire_failed_component(id);
        }
    }
}

bool CascaderComponentHost::editable(runtime::ComponentId id) const {
    auto s = state(*services_, id);
    return s && active_ && s->geometry_available && read_prop(s->props.visible_) && !read_prop(s->props.disabled_) &&
           !read_prop(s->props.read_only_) && services_->interactions().component_active(id);
}

void CascaderComponentHost::on_window_active(bool value) {
    active_ = value;
    on_input_boundary_changed();
}

void CascaderComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        refresh(id);
    }
}

CascaderSnapshot CascaderComponentHost::snapshot(runtime::ComponentId id) const {
    auto s = state(*services_, id);
    if (!s) {
        throw std::out_of_range("Cascader is stale");
    }
    CascaderSnapshot out{
        s->selector, s->search, s->panel, s->value, s->expanded, {}, s->popup_open.get() || s->embedded};
    for (const auto& r : s->rows) {
        out.rows.push_back(
            {r->option.key, r->path, r->component, r->shown, r->checked, r->disabled.get(), r->viewport});
    }
    out.status = s->status;
    out.retry = s->retry_button;
    out.error = s->error;
    out.loading = s->loading.has_value();
    out.clear = s->clear;
    out.arrow = s->arrow;
    out.overflow = s->overflow;
    out.scroll = s->scroll;
    for (auto& tag : s->tags) {
        out.tags.push_back({tag->path, tag->component, tag->remove, tag->label.get(), tag->shown});
    }
    return out;
}

void CascaderComponentHost::reconcile(runtime::ComponentId id, const std::vector<CascaderOption>& options) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    std::vector<String> keys;
    validate(options, keys);
    if (s->reconciling) {
        services_->retire_failed_component(id);
        throw std::logic_error("Cascader options reentered a content factory");
    }
    s->reconciling = true;

    struct ReconcileGuard {
        S s;

        ~ReconcileGuard() {
            s->reconciling = false;
        }
    } guard{s};

    run_untracked([&] {
        const auto old = s->rows;
        std::vector<std::shared_ptr<Row>> next;
        std::vector<runtime::ComponentId> added;
        try {
            const auto visit = [&](auto&& self, const std::vector<CascaderOption>& list, CascaderPath path,
                                   std::vector<CascaderOption> ancestry, bool blocked) -> void {
                for (auto x : list) {
                    auto p = path;
                    p.push_back(x.value);
                    auto a = ancestry;
                    a.push_back(x);
                    auto it = std::ranges::find_if(old, [&](const auto& r) {
                        return r->option.key == x.key && r->option.content.has_value() == x.content.has_value() &&
                               r->option.content_revision == x.content_revision;
                    });
                    auto r = it == old.end() ? std::make_shared<Row>() : *it;
                    if (it == old.end()) {
                        Content content{[&, r, option = x] {
                            auto& build = runtime::require_component_build_context();
                            r->viewport = build.mount_component<int>(0);
                            const auto node = build.root(r->viewport);
                            services_->nodes().require(node).clip_content = true;
                            services_->layout().set_layout(
                                node, layout::ComponentLayout{
                                          [this, r](auto& e, auto, auto c) {
                                              if (services_->components().contains(r->component)) {
                                                  (void)e.measure_child(
                                                      services_->components().root(r->component),
                                                      {0, c.max_width, 0, std::numeric_limits<float>::infinity()});
                                              }
                                              return c.constrain({c.max_width, 0});
                                          },
                                          [this, r](auto& e, auto, auto rect) {
                                              if (services_->components().contains(r->component)) {
                                                  e.place_child(services_->components().root(r->component),
                                                                {rect.x, r->row_y, rect.width, r->row_height}, true,
                                                                true);
                                              }
                                          }});
                            build.on_resource_cleanup(r->viewport,
                                                      [this, node] { services_->layout().remove_layout(node); });
                            build.mount_slot(
                                r->viewport, Content{[&, r, option] {
                                    Theme(
                                        ThemeProps{}.config(r->theme), ThemeContent{[&, r, option] {
                                            const auto before = services_->interactions().declaration_order().size();
                                            Button(
                                                ButtonProps{}
                                                    .type(ButtonType::Text)
                                                    .disabled(r->disabled)
                                                    .wave(false)
                                                    .ref(r->ref)
                                                    .onClick([this, id, key = option.key] { activate(id, key, true); }),
                                                ButtonContent{[this, r, option] {
                                                    Flex(FlexProps{}
                                                             .direction(r->flow)
                                                             .align(FlexAlign::Center)
                                                             .gap(dp(8)),
                                                         FlexContent{[this, r, option] {
                                                             Icon(IconProps{}
                                                                      .source(r->check_icon)
                                                                      .visible(r->check_visible)
                                                                      .twoToneColor(r->icon_color));
                                                             auto& build = runtime::require_component_build_context();
                                                             if (option.content) {
                                                                 r->rich_root = build.mount_component<int>(0);
                                                                 const auto node = build.root(r->rich_root);
                                                                 services_->layout().set_layout(node,
                                                                                                layout::BoxLayout{});
                                                                 build.on_resource_cleanup(r->rich_root, [this, node] {
                                                                     services_->layout().remove_layout(node);
                                                                 });
                                                                 build.mount_slot(r->rich_root, Content{[option] {
                                                                                      SlotContentAccess::function (
                                                                                          *option.content)();
                                                                                  }});
                                                             }
                                                             r->label_root = build.mount_component<int>(0);
                                                             const auto label_node = build.root(r->label_root);
                                                             services_->layout().set_layout(label_node,
                                                                                            layout::BoxLayout{});
                                                             build.on_resource_cleanup(
                                                                 r->label_root, [this, label_node] {
                                                                     services_->layout().remove_layout(label_node);
                                                                 });
                                                             build.mount_slot(r->label_root, Content{[r] {
                                                                                  Text(TypographyProps{}
                                                                                           .content(r->rich_label)
                                                                                           .direction(r->direction));
                                                                              }});
                                                             Icon(IconProps{}
                                                                      .source(r->icon)
                                                                      .visible(r->icon_visible)
                                                                      .twoToneColor(r->icon_color));
                                                         }});
                                                }});
                                            r->component =
                                                services_->interactions()
                                                    .require(services_->interactions().declaration_order()[before])
                                                    .component;
                                        }});
                                }});
                        }};
                        if (s->mounting) {
                            runtime::require_component_build_context().mount_slot(s->panel, content);
                        } else {
                            services_->append_slot(s->panel, content);
                        }
                        added.push_back(r->viewport);
                    }
                    // Validate the entire topology before committing any row metadata.
                    auto candidate = it == old.end() ? r : std::make_shared<Row>(*r);
                    candidate->option = x;
                    candidate->path = p;
                    candidate->ancestry = a;
                    candidate->column = path.size();
                    candidate->blocked = blocked || x.disabled;
                    next.push_back(candidate);
                    self(self, x.children, p, a, blocked || x.disabled);
                }
            };
            visit(visit, options, {}, {}, false);
        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
        for (const auto& r : old) {
            if (std::ranges::none_of(next, [&](const auto& n) { return n->component == r->component; })) {
                services_->destroy(r->viewport);
            }
        }
        if (!state(*services_, id)) {
            return;
        }
        // Retained closures refer to the original Row; commit metadata to those instances.
        for (auto& candidate : next) {
            auto found = std::ranges::find_if(old, [&](const auto& r) { return r->component == candidate->component; });
            if (found != old.end()) {
                (*found)->option = candidate->option;
                (*found)->path = candidate->path;
                (*found)->ancestry = candidate->ancestry;
                (*found)->column = candidate->column;
                (*found)->blocked = candidate->blocked;
                candidate = *found;
            }
        }
        if (s->retry_key &&
            std::ranges::none_of(next, [&](const auto& row) { return row->option.key == *s->retry_key; })) {
            s->retry_key.reset();
            s->error = {};
        }
        s->options = options;
        s->rows = std::move(next);
        ++s->revision;
        s->reconciling = false;
        refresh(id);
        on_focus_changed();
    });
}

void CascaderComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto s = state(*services_, id);
    if (!s || !s->ready || s->refreshing) {
        return;
    }
    s->refreshing = true;
    s->fitted_tags.reset();

    struct Guard {
        S s;

        ~Guard() {
            s->refreshing = false;
        }
    } guard{s};

    run_untracked([&] {
        try {
            services_->components().set_branch_active(id, read_prop(s->props.visible_));
            services_->set_input_blocked(s->content_root, !read_prop(s->props.visible_) ||
                                                              read_prop(s->props.disabled_) ||
                                                              read_prop(s->props.read_only_) || !active_);
            if (!state(*services_, id)) {
                return;
            }
            const bool enabled = editable(id);
            s->disabled.set(!enabled);
            if (!enabled) {
                if (!s->props.open_) {
                    s->open = false;
                }
                s->cancelled->store(true);
                ++s->generation;
                s->loading.reset();
            }
            if (!s->open && !s->embedded) {
                cancel_load(s);
            }
            s->popup_open.set(s->open && enabled);
            if (!state(*services_, id)) {
                return;
            }
            s->clear_disabled.set(!enabled || s->value.empty() || !read_prop(s->props.allow_clear_));
            if (s->clear.valid() && services_->components().contains(s->clear)) {
                const bool shown = !s->value.empty() && read_prop(s->props.allow_clear_);
                services_->components().set_branch_active(s->clear, shown);
                services_->nodes().require(services_->components().root(s->clear)).out_of_flow = !shown;
            }
            const auto token = services_->components().theme_scope(id)->snapshot().cascader();
            s->height = s->props.height_ ? read_prop(*s->props.height_) : token.metrics.dropdown_height;
            s->column_width = s->props.width_ ? read_prop(*s->props.width_) : token.metrics.column_width;
            if (!std::isfinite(s->height) || s->height <= 0 || !std::isfinite(s->column_width) ||
                s->column_width <= 0) {
                services_->retire_failed_component(id);
                throw std::invalid_argument("Cascader dimensions must be finite and positive");
            }
            auto visual_theme = child_theme(token, read_prop(s->props.size_));
            if (read_prop(s->props.show_search_) && s->selector_focused && s->query.empty() &&
                !read_prop(s->props.multiple_) && !s->value.empty()) {
                visual_theme.input.tokens.placeholder_color = token.colors.selector_color;
            }
            s->theme.set(visual_theme);
            const auto direction = read_prop(s->props.direction_);
            s->flow.set(direction == TextDirection::RightToLeft ? FlexDirection::RightToLeft
                                                                : FlexDirection::LeftToRight);
            s->placement.set(!s->props.explicit_placement_ && direction == TextDirection::RightToLeft
                                 ? PopoverPlacement::BottomRight
                                 : read_prop(s->props.placement_));
            s->overflow_theme.set(
                item_theme(sized_tags(token, read_prop(s->props.size_)), false, true,
                           std::ranges::find(services_->components().theme_scope(id)->snapshot().algorithms(),
                                             ThemeAlgorithm::Compact) !=
                               services_->components().theme_scope(id)->snapshot().algorithms().end()));
            ThemeConfig empty_theme;
            empty_theme.text.tokens.color = token.colors.empty;
            s->empty_theme.set(empty_theme);
            s->arrow_color.set({enabled ? token.colors.arrow : token.colors.disabled_color, {}});
            s->clear_color.set({enabled ? token.colors.clear : token.colors.disabled_color, {}});
            s->placeholder.set(read_prop(s->props.multiple_) && !s->value.empty() ? String{}
                                                                                  : read_prop(s->props.placeholder_));
            const bool show_search = read_prop(s->props.show_search_);
            s->arrow_source.set(show_search && s->open && enabled ? read_prop(s->props.search_icon_)
                                                                  : read_prop(s->props.suffix_icon_));
            s->selector_read_only.set(!show_search || read_prop(s->props.read_only_));
            s->effective_query = show_search ? s->query : String{};
            services_->components().set_branch_active(s->search, show_search);
            services_->nodes().require(services_->components().root(s->search)).out_of_flow = !show_search;
            std::string display;
            for (const auto& p : s->value) {
                const auto it = std::ranges::find_if(s->rows, [&](const auto& r) { return r->path == p; });
                if (it != s->rows.end()) {
                    if (!display.empty()) {
                        display += "; ";
                    }
                    display += join((*it)->ancestry).bytes();
                } else {
                    if (!display.empty()) {
                        display += "; ";
                    }
                    display += raw_label(p).bytes();
                }
            }
            const auto display_value = s->value;
            const auto refresh_revision = s->revision;
            invoke(*services_, id, [&] {
                s->display.set(read_prop(s->props.multiple_) && !s->value.empty() ? String{}
                               : s->props.display_                                ? s->props.display_(display_value)
                                                                                  : String::from_utf8(display).value());
            });
            if (s->revision != refresh_revision) {
                services_->retire_failed_component(id);
                return;
            }
            if (!state(*services_, id)) {
                return;
            }
            s->selector_text.set(show_search && (s->selector_focused || !s->query.empty()) ? s->query
                                                                                           : s->display.get());
            if (show_search && s->selector_focused && s->query.empty() && !read_prop(s->props.multiple_) &&
                !s->value.empty()) {
                s->placeholder.set(s->display.get());
            }
            std::vector<std::shared_ptr<Row>> matches;
            std::vector<std::size_t> orders(34);
            const auto revision = s->revision;
            const auto rows = s->rows;
            for (auto& r : rows) {
                r->checked = std::ranges::any_of(s->value, [&](const auto& p) { return prefix(p, r->path); });
                if (!r->option.children.empty() && read_prop(s->props.multiple_)) {
                    bool any = false;
                    bool all = true;
                    for (const auto& leaf : rows) {
                        if (leaf->option.children.empty() && !leaf->blocked && !leaf->option.disable_checkbox &&
                            prefix(r->path, leaf->path)) {
                            any = true;
                            all &= std::ranges::any_of(s->value, [&](const auto& p) { return prefix(p, leaf->path); });
                        }
                    }
                    if (any) {
                        r->checked = all;
                    }
                }
                r->flow.set(s->flow.get());
                r->direction.set(direction);
                r->check_visible.set(read_prop(s->props.multiple_));
                const bool partial = std::ranges::any_of(s->value, [&](const auto& p) { return prefix(r->path, p); });
                r->check_icon.set(IconSource{r->checked ? IconName::CheckSquareOutlined
                                             : partial  ? IconName::MinusSquareOutlined
                                                        : IconName::BorderOutlined});
                r->icon_color.set({r->option.disable_checkbox ? token.colors.disabled_color : token.colors.arrow, {}});
                auto parent = r->path;
                parent.pop_back();
                r->shown = parent.empty() || prefix(parent, s->expanded);
                if (!s->effective_query.empty()) {
                    r->shown =
                        (r->option.children.empty() ||
                         (!read_prop(s->props.multiple_) && read_prop(s->props.change_on_select_))) &&
                        (r->option.is_leaf || !r->option.children.empty() || !s->props.loader_) &&
                        (s->props.search_.filter ? s->props.search_.filter(s->effective_query, r->ancestry)
                                                 : input::text_search_key(join(r->ancestry).bytes())
                                                           .find(input::text_search_key(s->effective_query.bytes())) !=
                                                       std::string_view::npos);
                    if (r->shown) {
                        matches.push_back(r);
                    }
                }
                if (!state(*services_, id)) {
                    return;
                }
                if (revision != s->revision) {
                    services_->retire_failed_component(id);
                    return;
                }
                r->label.set(!s->effective_query.empty() && s->props.search_.render
                                 ? s->props.search_.render(s->effective_query, r->ancestry)
                             : !s->effective_query.empty()
                                 ? join(r->ancestry)
                                 : (r->option.label.empty() ? raw_label({r->option.value}) : r->option.label));
                if (!state(*services_, id)) {
                    return;
                }
                if (revision != s->revision) {
                    services_->retire_failed_component(id);
                    return;
                }
                r->rich_label.set(highlighted(r->label.get(), s->props.search_.render ? String{} : s->effective_query));
                const bool rich = r->rich_root.valid() && s->effective_query.empty();
                if (r->rich_root.valid()) {
                    services_->components().set_branch_active(r->rich_root, rich);
                    services_->nodes().require(services_->components().root(r->rich_root)).out_of_flow = !rich;
                }
                services_->components().set_branch_active(r->label_root, !rich);
                services_->nodes().require(services_->components().root(r->label_root)).out_of_flow = rich;
                r->disabled.set(!enabled || r->blocked);
                r->theme.set(
                    item_theme(token, r->checked || s->expanded == r->path, false,
                               std::ranges::find(services_->components().theme_scope(id)->snapshot().algorithms(),
                                                 ThemeAlgorithm::Compact) !=
                                   services_->components().theme_scope(id)->snapshot().algorithms().end()));
                r->icon_visible.set(!r->option.children.empty() || !r->option.is_leaf);
                r->icon.set(s->loading && *s->loading == r->option.key ? read_prop(s->props.loading_icon_)
                            : !s->props.explicit_expand_icon_ && direction == TextDirection::RightToLeft
                                ? IconSource{IconName::LeftOutlined}
                                : read_prop(s->props.expand_icon_));
                // Hide the expansion icon on actual leaf rows through its own retained component below.
            }
            if (s->props.search_.sort && !s->effective_query.empty()) {
                std::stable_sort(matches.begin(), matches.end(), [&](const auto& a, const auto& b) {
                    return s->props.search_.sort(a->path, b->path, s->effective_query);
                });
            }
            if (!state(*services_, id)) {
                return;
            }
            if (revision != s->revision) {
                services_->retire_failed_component(id);
                return;
            }
            if (!s->effective_query.empty()) {
                for (auto& r : s->rows) {
                    r->shown = false;
                }
                for (std::size_t i = 0; i < matches.size() && (!s->props.search_.limit || i < s->props.search_.limit);
                     ++i) {
                    matches[i]->shown = true;
                    matches[i]->order = i;
                }
            }
            bool any = false;
            for (auto& r : s->rows) {
                if (s->effective_query.empty()) {
                    r->order = orders[r->column]++;
                }
                any |= r->shown;
                services_->components().set_branch_active(r->viewport, r->shown);
                services_->nodes().require(services_->components().root(r->viewport)).out_of_flow = !r->shown;
            }
            const bool status = s->loading.has_value() || !s->error.empty();
            services_->components().set_branch_active(s->status, status);
            services_->nodes().require(services_->components().root(s->status)).out_of_flow = !status;
            services_->components().set_branch_active(s->empty, !any && !status);
            services_->nodes().require(services_->components().root(s->empty)).out_of_flow = any || status;
            const auto retry_row =
                s->retry_key
                    ? std::ranges::find_if(s->rows, [&](const auto& row) { return row->option.key == *s->retry_key; })
                    : s->rows.end();
            const bool retry = !s->error.empty() && retry_row != s->rows.end();
            s->retry_disabled.set(!enabled || !retry || (retry_row != s->rows.end() && (*retry_row)->blocked));
            services_->components().set_branch_active(s->retry_button, retry);
            services_->nodes().require(services_->components().root(s->retry_button)).out_of_flow = !retry;
            s->empty_label.set(s->loading ? String{u8"Loading…"} : s->error);
            reconcile_tags(id);
            if (!state(*services_, id)) {
                return;
            }
            services_->mark_scene_structure_dirty();
            if (geometry) {
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                           runtime::DirtyFlags::HitTest);
                services_->dirty().invalidate(services_->components().root(s->panel), runtime::DirtyFlags::Measure |
                                                                                          runtime::DirtyFlags::Layout |
                                                                                          runtime::DirtyFlags::HitTest);
            } else {
                services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material | runtime::DirtyFlags::Geometry);
            }

        } catch (...) {
            services_->retire_failed_component(id);
            throw;
        }
    });
}

void CascaderComponentHost::request_open(runtime::ComponentId id, bool value) {
    auto s = state(*services_, id);
    if (!s || (value && !editable(id))) {
        return;
    }
    if (!value) {
        s->cancelled->store(true);
        ++s->generation;
        s->loading.reset();
    }
    if (value && !s->props.expanded_ && !s->value.empty()) {
        s->expanded = s->value.front();
    }
    if (!s->props.open_) {
        if (s->open != value) {
            ++s->revision;
        }
        s->open = value;
    }
    refresh(id);
    const auto cb = s->props.on_open_;
    if (cb) {
        invoke(*services_, id, [&] { cb(value); });
    }
}

void CascaderComponentHost::change(runtime::ComponentId id, CascaderPaths value) {
    auto s = state(*services_, id);
    if (!s || !editable(id) || s->value == value) {
        return;
    }
    if (!s->props.value_) {
        s->value = value;
        ++s->revision;
    }
    refresh(id);
    const auto cb = s->props.on_change_;
    const auto detail = s->props.on_change_options_;
    if (detail && state(*services_, id)) {
        CascaderSelectedOptions selected;
        for (const auto& path : value) {
            const auto row = std::ranges::find_if(s->rows, [&](const auto& item) { return item->path == path; });
            selected.push_back(row == s->rows.end() ? std::vector<CascaderOption>{} : (*row)->ancestry);
        }
        invoke(*services_, id, [&] { detail(std::move(value), std::move(selected)); });
    } else if (cb && state(*services_, id)) {
        invoke(*services_, id, [&] { cb(std::move(value)); });
    }
}

void CascaderComponentHost::activate(runtime::ComponentId id, String key, bool select) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    auto it = std::ranges::find_if(s->rows, [&](const auto& r) { return r->option.key == key; });
    if (it == s->rows.end() || (*it)->blocked) {
        return;
    }
    auto r = *it;
    s->error = {};
    s->retry_key.reset();
    s->cancelled->store(true);
    ++s->generation;
    s->loading.reset();
    if (!s->props.expanded_) {
        if (s->expanded != r->path) {
            ++s->revision;
        }
        s->expanded = r->path;
        s->reveal_columns = true;
    }
    refresh(id);
    const auto revision = s->revision;
    const auto expand = s->props.on_expand_;
    if (expand) {
        invoke(*services_, id, [&] { expand(r->path); });
    }
    if (!state(*services_, id) || revision != s->revision) {
        return;
    }
    if (r->option.children.empty() && !r->option.is_leaf && s->props.loader_) {
        load(id, key);
        return;
    }
    if (!select ||
        (!read_prop(s->props.multiple_) && !read_prop(s->props.change_on_select_) && !r->option.children.empty())) {
        return;
    }
    auto next = s->value;
    if (read_prop(s->props.multiple_)) {
        if (r->option.disable_checkbox) {
            return;
        }
        next.clear();
        for (const auto& leaf : s->rows) {
            if (!leaf->option.children.empty()) {
                continue;
            }
            bool checked = std::ranges::any_of(s->value, [&](const auto& p) { return prefix(p, leaf->path); });
            if (prefix(r->path, leaf->path) && !leaf->blocked && !leaf->option.disable_checkbox) {
                checked = !r->checked;
            }
            if (checked) {
                next.push_back(leaf->path);
            }
        }
        if (read_prop(s->props.strategy_) == CascaderCheckedStrategy::ShowParent) {
            for (auto rit = s->rows.rbegin(); rit != s->rows.rend(); ++rit) {
                auto parent = *rit;
                if (parent->option.children.empty() || parent->blocked || parent->option.disable_checkbox) {
                    continue;
                }
                const bool all = std::ranges::all_of(s->rows, [&](const auto& x) {
                    return !prefix(parent->path, x->path) || !x->option.children.empty() ||
                           std::ranges::any_of(next, [&](const auto& p) { return prefix(p, x->path); });
                });
                if (all) {
                    std::erase_if(next, [&](const auto& p) { return prefix(parent->path, p); });
                    next.push_back(parent->path);
                }
            }
        }
    } else {
        next = {r->path};
    }
    change(id, std::move(next));
    if (state(*services_, id) && !s->effective_query.empty() &&
        (!read_prop(s->props.multiple_) || s->props.search_.auto_clear_search_value)) {
        set_query(id, {}, true);
    }
    if (state(*services_, id) && !read_prop(s->props.multiple_) && !s->embedded) {
        request_open(id, false);
    }
}

void CascaderComponentHost::load(runtime::ComponentId id, String key) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    const auto found =
        std::ranges::find_if(s->rows, [&](const auto& row) { return row->option.key == key && !row->blocked; });
    if (found == s->rows.end()) {
        return;
    }
    auto row = *found;
    s->retry_key.reset();
    s->cancelled->store(true);
    s->cancelled = std::make_shared<std::atomic_bool>(false);
    const auto ticket = ++s->generation;
    s->loading = key;
    s->error = String{};
    refresh(id);
    if (!state(*services_, id) || !editable(id) || s->generation != ticket || s->cancelled->load()) {
        return;
    }
    const auto loader = s->props.loader_;
    const auto completion = [this, weak = std::weak_ptr{s}, id, key, ticket](CascaderLoadResult result) {
        auto s = weak.lock();
        if (!s || s->thread != std::this_thread::get_id() || s->cancelled->load() || s->generation != ticket) {
            return false;
        }
        if (!state(*services_, id) || !editable(id)) {
            return false;
        }
        s->cancelled->store(true);
        s->loading.reset();
        if (!result.error.empty()) {
            s->error = result.error;
            s->retry_key = key;
            refresh(id);
            return true;
        }
        auto options = s->options;
        const auto patch = [&](auto&& self, std::vector<CascaderOption>& list) -> void {
            for (auto& x : list) {
                if (x.key == key) {
                    x.children = result.children;
                    x.is_leaf = x.children.empty();
                } else {
                    self(self, x.children);
                }
            }
        };
        patch(patch, options);
        try {
            reconcile(id, options);
        } catch (...) {
            if (state(*services_, id)) {
                s->error = String{u8"Loading failed"};
                s->retry_key = key;
                refresh(id);
            }
            throw;
        }

        return true;
    };
    try {
        loader({row->path, s->cancelled, completion});
    } catch (...) {
        if (state(*services_, id)) {
            s->cancelled->store(true);
            ++s->generation;
            s->loading.reset();
            s->error = String{u8"Loading failed"};
            s->retry_key = key;
            refresh(id);
        }
        throw;
    }
}

void CascaderComponentHost::mount(const CascaderProps& p, bool embedded) {
    FormPrivateEditorScope private_editor;
    if ((p.query_ && p.default_query_) || (p.value_ && p.default_value_) || (p.open_ && p.default_open_) ||
        (p.expanded_ && p.default_expanded_)) {
        throw std::invalid_argument("Cascader controlled/default conflict");
    }
    std::vector<String> keys;
    validate(read_prop(p.options_), keys);
    if (!std::isfinite(p.height_ ? read_prop(*p.height_) : 180.F) || (p.height_ ? read_prop(*p.height_) : 180.F) <= 0 ||
        !std::isfinite(p.width_ ? read_prop(*p.width_) : 111.F) || (p.width_ ? read_prop(*p.width_) : 111.F) <= 0) {
        throw std::invalid_argument("Cascader geometry must be finite and positive");
    }
    auto s = std::make_shared<State>();
    s->props = p;
    s->query = p.query_ ? read_prop(*p.query_) : p.default_query_.value_or(String{});
    s->query_signal.set(s->query);
    s->selector_read_only.set(!read_prop(p.show_search_) || read_prop(p.read_only_));
    s->embedded = embedded;
    s->value = p.value_ ? read_prop(*p.value_) : p.default_value_.value_or(CascaderPaths{});
    validate_paths(s->value, read_prop(p.multiple_));
    s->open = p.open_ ? read_prop(*p.open_) : p.default_open_.value_or(false);
    s->popup_open.set(s->open && read_prop(p.visible_) && !read_prop(p.disabled_) && !read_prop(p.read_only_) &&
                      active_);
    s->expanded = p.expanded_ ? read_prop(*p.expanded_)
                              : p.default_expanded_.value_or(s->value.empty() ? CascaderPath{} : s->value.front());
    if (!s->expanded.empty()) {
        validate_paths({s->expanded}, false);
    }
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<S>(s);
    s->node = b.root(id);

    struct Transaction {
        WindowComponentServices& v;
        runtime::ComponentId id;
        bool done{};

        ~Transaction() {
            if (!done) {
                v.retire_failed_component(id);
            }
        }
    } transaction{*services_, id};

    b.on_resource_cleanup(id, [this, s] {
        s->cancelled->store(true);
        s->bindings.dispose();
        services_->layout().remove_layout(s->node);
        if (services_->components().contains(s->panel)) {
            services_->layout().remove_layout(services_->components().root(s->panel));
        }
    });
    services_->layout().set_layout(s->node, layout::BoxLayout{{}, embedded});
    runtime::connect_layout_style(s->bindings, p.layout_, s->node, services_->nodes(), services_->dirty());
    const auto panel = [&] {
        auto& build = runtime::require_component_build_context();
        s->panel = build.mount_component<int>(0);
        const auto node = build.root(s->panel);
        services_->nodes().require(node).clip_content = true;
        const auto fragment = build.register_scene_fragment(s->panel, runtime::SceneFragmentPlacement::before_children);
        s->background = services_->surfaces().create_content_range(fragment, {});
        build.on_resource_cleanup(
            s->panel, [this, surface = s->background] { services_->surfaces().destroy_content_range(surface); });
        build.mount_slot(
            s->panel, Content{[&] {
                Input(InputProps{}
                          .value(s->query_signal)
                          .disabled(s->disabled)
                          .placeholder(String{u8"Search paths"})
                          .direction(p.direction_)
                          .onChange([this, id](String q) { set_query(id, q, true); }),
                      InputPrefix{[&] { Icon(IconProps{}.source(p.search_icon_).twoToneColor(s->arrow_color)); }});
                s->search = services_->input_runtime()->mounted_inputs().back().component;
                {
                    auto& status_build = runtime::require_component_build_context();
                    s->status = status_build.mount_component<int>(0);
                    const auto status_node = status_build.root(s->status);
                    services_->nodes().require(status_node).clip_content = true;
                    services_->layout().set_layout(status_node, layout::BoxLayout{});
                    status_build.on_resource_cleanup(
                        s->status, [this, status_node] { services_->layout().remove_layout(status_node); });
                    status_build.mount_slot(
                        s->status, Content{[&] {
                            Theme(ThemeProps{}.config(s->empty_theme), ThemeContent{[&] {
                                      Flex(FlexProps{}
                                               .vertical(true)
                                               .direction(s->flow)
                                               .align(FlexAlign::Center)
                                               .gap(dp(8)),
                                           FlexContent{[&] {
                                               Text(TextProps{}.content(s->empty_label));
                                               const auto before = services_->interactions().declaration_order().size();
                                               Button(ButtonProps{}
                                                          .type(ButtonType::Link)
                                                          .disabled(s->retry_disabled)
                                                          .wave(false)
                                                          .onClick([this, id] {
                                                              if (auto s = state(*services_, id); s && s->retry_key) {
                                                                  load(id, *s->retry_key);
                                                              }
                                                          }),
                                                      ButtonContent{[] { Text(u8"Retry"); }});
                                               s->retry_button =
                                                   services_->interactions()
                                                       .require(services_->interactions().declaration_order()[before])
                                                       .component;
                                           }});
                                  }});
                        }});
                }
                Theme(ThemeProps{}.config(s->empty_theme), ThemeContent{[&] {
                          if (p.empty_content_) {
                              SlotContentAccess::function (*p.empty_content_)();
                          } else {
                              Text(TextProps{}.content(p.empty_));
                          }
                      }});
                s->empty = services_->components().children(s->panel).back();
                if (p.popup_) {
                    auto& b = runtime::require_component_build_context();
                    s->footer = b.mount_component<int>(0);
                    const auto node = b.root(s->footer);
                    services_->nodes().require(node).clip_content = true;
                    services_->layout().set_layout(node, layout::BoxLayout{});
                    b.on_resource_cleanup(s->footer, [this, node] { services_->layout().remove_layout(node); });
                    b.mount_slot(s->footer, Content{[&] { SlotContentAccess::function (*p.popup_)(); }});
                }
            }});
        services_->layout().set_layout(
            node, layout::ComponentLayout{[this, id](auto& e, auto, auto c) { return measure_panel(id, e, c); },
                                          [this, id](auto& e, auto, auto rect) { place_panel(id, e, rect); }});
    };
    b.mount_slot(
        id, Content{[&] {
            Theme(
                ThemeProps{}.config(s->theme), ThemeContent{[&] {
                    if (embedded) {
                        panel();
                        return;
                    }
                    Popover(
                        PopoverProps{}
                            .trigger(PopoverTriggerMode::Manual)
                            .placement(s->placement)
                            .arrow(false)
                            .open(s->popup_open)
                            .onOpenChange([this, id](bool open) { request_open(id, open); }),
                        PopoverTrigger{[&] {
                            auto& b = runtime::require_component_build_context();
                            s->selector_wrapper = b.mount_component<int>(0);
                            const auto node = b.root(s->selector_wrapper);
                            b.on_resource_cleanup(s->selector_wrapper,
                                                  [this, node] { services_->layout().remove_layout(node); });
                            b.mount_slot(
                                s->selector_wrapper, Content{[&] {
                                    if (p.trigger_) {
                                        const auto before = services_->interactions().declaration_order().size();
                                        Button(ButtonProps{}
                                                   .disabled(s->disabled)
                                                   .wave(false)
                                                   .ref(p.reference_.button_)
                                                   .onClick([this, id] {
                                                       if (auto s = state(*services_, id)) {
                                                           request_open(id, !s->open);
                                                       }
                                                   }),
                                               ButtonContent{[&] { SlotContentAccess::function (*p.trigger_)(); }});
                                        s->selector =
                                            services_->interactions()
                                                .require(services_->interactions().declaration_order()[before])
                                                .component;
                                    } else {
                                        Input(
                                            InputProps{}
                                                .value(s->selector_text)
                                                .placeholder(s->placeholder)
                                                .readOnly(s->selector_read_only)
                                                .disabled(s->disabled)
                                                .size(p.size_)
                                                .status(p.status_)
                                                .variant(p.variant_)
                                                .direction(p.direction_)
                                                .ref(p.reference_.input_)
                                                .onChange([this, id](String query) {
                                                    set_query(id, std::move(query), true);
                                                    if (auto s = state(*services_, id); s && !s->open) {
                                                        request_open(id, true);
                                                    }
                                                })
                                                .onFocus([this, id] { emit_focus(id, true); })
                                                .onBlur([this, id] { emit_focus(id, false); }),
                                            InputPrefix{[&] {
                                                Flex(
                                                    FlexProps{}.direction(s->flow).align(FlexAlign::Center),
                                                    FlexContent{[&] {
                                                        if (p.prefix_) {
                                                            SlotContentAccess::function (*p.prefix_)();
                                                        }
                                                        auto& spacer_build = runtime::require_component_build_context();
                                                        s->tag_spacer = spacer_build.mount_component<int>(0);
                                                        const auto spacer = spacer_build.root(s->tag_spacer);
                                                        spacer_build.on_resource_cleanup(s->tag_spacer, [this, spacer] {
                                                            services_->layout().remove_layout(spacer);
                                                        });
                                                        services_->layout().set_layout(
                                                            spacer,
                                                            layout::ComponentLayout{[s](auto&, auto, auto c) {
                                                                                        return c.constrain(
                                                                                            {s->tag_inline_width, 0});
                                                                                    },
                                                                                    [](auto&, auto, auto) {}});
                                                    }});
                                            }},
                                            InputSuffix{[&] {
                                                Flex(FlexProps{}.direction(s->flow).align(FlexAlign::Center).gap(dp(4)),
                                                     FlexContent{[&] {
                                                         const auto before =
                                                             services_->interactions().declaration_order().size();
                                                         Button(ButtonProps{}
                                                                    .type(ButtonType::Text)
                                                                    .disabled(s->clear_disabled)
                                                                    .wave(false)
                                                                    .onClick([this, id] { clear(id); }),
                                                                ButtonContent{[&] {
                                                                    Icon(IconProps{}
                                                                             .source(p.clear_icon_)
                                                                             .twoToneColor(s->clear_color));
                                                                }});
                                                         s->clear = services_->interactions()
                                                                        .require(services_->interactions()
                                                                                     .declaration_order()[before])
                                                                        .component;
                                                         Icon(IconProps{}
                                                                  .source(s->arrow_source)
                                                                  .twoToneColor(s->arrow_color));
                                                         s->arrow = services_->text().mounted_texts().back().component;
                                                     }});
                                            }});
                                        // The selector can contain rich prefix Inputs; identify it by its public
                                        // reference's owning mount.
                                        for (const auto& input : services_->input_runtime()->mounted_inputs()) {
                                            if (descendant(*services_, input.component, s->selector_wrapper) &&
                                                (!s->selector.valid() ||
                                                 descendant(*services_, s->selector, input.component))) {
                                                s->selector = input.component;
                                            }
                                        }
                                    }
                                    mount_tags(id);
                                }});
                            services_->layout().set_layout(
                                node,
                                layout::ComponentLayout{
                                    [this, id](auto& e, auto, auto c) {
                                        auto s = state(*services_, id);
                                        if (!s) {
                                            return runtime::Size{};
                                        }
                                        const auto t = services_->components().theme_scope(id)->snapshot().cascader();
                                        const auto& external = services_->nodes().require(s->node).external_layout;
                                        const float desired =
                                            external.width.value_or(external.flex_grow > 0 && std::isfinite(c.max_width)
                                                                        ? c.max_width
                                                                        : t.metrics.control_width);
                                        const float width = std::clamp(desired, c.min_width, c.max_width);
                                        const float query_width = read_prop(s->props.show_search_) ? 40.F : 0.F;
                                        const float tag_width =
                                            std::max(0.F, width - 2 * t.metrics.selector_padding - 64 - query_width);
                                        s->available_tag_width = tag_width;
                                        const auto tags =
                                            e.measure_child(services_->components().root(s->tag_root),
                                                            {0, tag_width, 0, std::numeric_limits<float>::infinity()});
                                        s->tag_inline_width = tags.width;
                                        const auto input = e.measure_child(services_->components().root(s->selector),
                                                                           {0, width, 0, c.max_height});
                                        return c.constrain(
                                            {width, std::max(input.height, tags.height + 2 * t.metrics.tag_gap)});
                                    },
                                    [this, id](auto& e, auto, auto rect) {
                                        auto s = state(*services_, id);
                                        if (!s) {
                                            return;
                                        }
                                        const auto t = services_->components().theme_scope(id)->snapshot().cascader();
                                        e.place_child(services_->components().root(s->selector), rect, true, true);
                                        const auto spacer =
                                            s->tag_spacer.valid()
                                                ? services_->nodes()
                                                      .require(services_->components().root(s->tag_spacer))
                                                      .bounds
                                                : runtime::Rect{rect.x + t.metrics.selector_padding, rect.y,
                                                                s->tag_inline_width, rect.height};
                                        e.place_child(services_->components().root(s->tag_root),
                                                      {spacer.x, rect.y + t.metrics.tag_gap, spacer.width,
                                                       std::max(0.F, rect.height - 2 * t.metrics.tag_gap)},
                                                      true, true);
                                    }});
                        }},
                        PopoverSlots{{}, PopoverContent{panel}});
                }});
        }});
    s->content_root = services_->components().children(id).front();
    const auto theme = b.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::cascader_colors, &theme_runtime::ThemeScope::cascader_metrics,
        &theme_runtime::ThemeScope::cascader_typography, &theme_runtime::ThemeScope::cascader_effects};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s->subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2, effects = i == 3](auto) {
                if (auto s = state(*services_, id); s && (s->refreshing || s->reconciling || s->measuring_tags)) {
                    ++s->revision;
                }
                refresh(id, geometry);
                if (auto s = state(*services_, id); s && effects) {
                    const auto flags =
                        runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest;
                    services_->dirty().invalidate(s->node, flags);
                    services_->dirty().invalidate(services_->components().root(s->panel), flags);
                }
            },
            [theme, a = accessors[i]] { (void)((theme.get()->*a)()); });
    }
    s->ready = true;
    reconcile(id, read_prop(p.options_));
    connect_prop(
        s->bindings, p.options_,
        [this, id](auto x) {
            if (auto s = state(*services_, id)) {
                s->cancelled->store(true);
                ++s->generation;
                s->loading.reset();
            }
            reconcile(id, x);
        },
        [](const auto&, const auto&) { return false; });
    if (p.value_) {
        connect_prop(s->bindings, *p.value_, [this, id](auto x) {
            if (auto s = state(*services_, id)) {
                validate_paths(x, read_prop(s->props.multiple_));
                s->value = std::move(x);
                ++s->revision;
                refresh(id);
            }
        });
    }
    if (p.open_) {
        connect_prop(s->bindings, *p.open_, [this, id](bool x) {
            if (auto s = state(*services_, id)) {
                s->open = x;
                ++s->revision;
                refresh(id);
            }
        });
    }
    if (p.expanded_) {
        connect_prop(s->bindings, *p.expanded_, [this, id](auto x) {
            if (auto s = state(*services_, id)) {
                if (!x.empty()) {
                    validate_paths({x}, false);
                }
                s->expanded = std::move(x);
                s->reveal_columns = true;
                ++s->revision;
                refresh(id);
            }
        });
    }
    const auto watch = [&]<class T>(const Prop<T>& prop) {
        connect_prop(s->bindings, prop, [this, id](const auto&) {
            if (auto s = state(*services_, id); s && (s->refreshing || s->reconciling || s->measuring_tags)) {
                ++s->revision;
            }
            refresh(id);
        });
    };
    watch(p.disabled_);
    watch(p.read_only_);
    watch(p.visible_);
    connect_prop(s->bindings, p.multiple_, [this, id](bool multiple) {
        if (auto s = state(*services_, id)) {
            invoke(*services_, id, [&] { validate_paths(s->value, multiple); });
            ++s->revision;
            refresh(id);
        }
    });
    watch(p.strategy_);
    watch(p.direction_);
    watch(p.placement_);
    if (p.height_) {
        watch(*p.height_);
    }
    if (p.width_) {
        watch(*p.width_);
    }
    watch(p.allow_clear_);
    watch(p.show_search_);
    watch(p.suffix_icon_);
    watch(p.search_icon_);
    watch(p.size_);
    watch(p.status_);
    watch(p.variant_);
    watch(p.empty_);
    if (p.max_tags_) {
        watch(*p.max_tags_);
    }
    if (p.max_tag_text_) {
        watch(*p.max_tag_text_);
    }
    if (p.query_) {
        connect_prop(s->bindings, *p.query_, [this, id](String q) { set_query(id, q, false); });
    }
    mounted_.push_back(id);
    refresh(id);
    s->mounting = false;
    transaction.done = true;
}

void CascaderComponentHost::set_query(runtime::ComponentId id, String q, bool notify) {
    auto s = state(*services_, id);
    if (!s || (notify && !editable(id))) {
        return;
    }
    cancel_load(s);
    s->error = {};
    s->retry_key.reset();
    if (!notify || !s->props.query_) {
        if (s->query != q) {
            ++s->revision;
        }
        s->query = q;
        s->scroll.assign(34, 0);
        s->reveal_columns = true;
    }
    s->query_signal.set(s->query);
    refresh(id);
    const auto cb = s->props.on_search_;
    if (notify && cb && state(*services_, id)) {
        invoke(*services_, id, [&] { cb(q); });
    }
}

void CascaderComponentHost::emit_focus(runtime::ComponentId id, bool focused) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    if (s->selector_focused != focused) {
        s->selector_focused = focused;
        refresh(id);
        if (!state(*services_, id)) {
            return;
        }
    }
    const auto cb = focused ? s->props.on_focus_ : s->props.on_blur_;
    if (cb) {
        invoke(*services_, id, cb);
    }
}

void CascaderComponentHost::clear(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s || !editable(id) || !read_prop(s->props.allow_clear_)) {
        return;
    }
    change(id, {});
    if (state(*services_, id) && !s->query.empty()) {
        set_query(id, {}, true);
    }
    if (state(*services_, id) && s->props.on_clear_) {
        invoke(*services_, id, s->props.on_clear_);
    }
}

void CascaderComponentHost::remove_tag(runtime::ComponentId id, CascaderPath path) {
    auto s = state(*services_, id);
    if (!s || !editable(id)) {
        return;
    }
    const auto row = std::ranges::find_if(s->rows, [&](auto& r) { return r->path == path; });
    if (row != s->rows.end() && ((*row)->blocked || (*row)->option.disable_checkbox)) {
        return;
    }
    auto next = s->value;
    std::erase_if(next, [&](const auto& p) { return prefix(path, p); });
    // SHOW_CHILD can display descendants of a parent-valued controlled selection.
    for (const auto& p : s->value) {
        if (p != path && prefix(p, path)) {
            std::erase(next, p);
            for (const auto& r : s->rows) {
                if (r->option.children.empty() && prefix(p, r->path) && !prefix(path, r->path)) {
                    next.push_back(r->path);
                }
            }
        }
    }
    for (const auto& leaf : s->rows) {
        if (leaf->option.children.empty() && (leaf->blocked || leaf->option.disable_checkbox) &&
            prefix(path, leaf->path) &&
            std::ranges::any_of(s->value, [&](const auto& p) { return prefix(p, leaf->path); }) &&
            !std::ranges::any_of(next, [&](const auto& p) { return prefix(p, leaf->path); })) {
            next.push_back(leaf->path);
        }
    }
    change(id, std::move(next));
}

void CascaderComponentHost::mount_tags(runtime::ComponentId id) {
    auto s = state(*services_, id);
    auto& b = runtime::require_component_build_context();
    s->tag_root = b.mount_component<int>(0);
    const auto node = b.root(s->tag_root);
    services_->nodes().require(node).clip_content = true;
    b.on_resource_cleanup(s->tag_root, [this, node] { services_->layout().remove_layout(node); });
    b.mount_slot(
        s->tag_root, Content{[&] {
            Theme(ThemeProps{}.config(s->overflow_theme), ThemeContent{[&] {
                      Popover(
                          PopoverProps{}.trigger(PopoverTriggerMode::Click).arrow(false).disabled(s->overflow_disabled),
                          PopoverTrigger{[&] {
                              const auto before = services_->interactions().declaration_order().size();
                              Button(ButtonProps{}.disabled(s->overflow_disabled).wave(false),
                                     ButtonContent{[s] { Text(TextProps{}.content(s->overflow_label)); }});
                              s->overflow = services_->interactions()
                                                .require(services_->interactions().declaration_order()[before])
                                                .component;
                          }},
                          PopoverSlots{{}, PopoverContent{[&] {
                                           auto& b = runtime::require_component_build_context();
                                           s->overflow_content = b.mount_component<int>(0);
                                           const auto node = b.root(s->overflow_content);
                                           services_->nodes().require(node).clip_content = true;
                                           services_->layout().set_layout(node, layout::BoxLayout{});
                                           b.on_resource_cleanup(s->overflow_content, [this, node] {
                                               services_->layout().remove_layout(node);
                                           });
                                       }}});
                  }});
        }});
    services_->layout().set_layout(
        node,
        layout::ComponentLayout{
            [this, id](auto& e, auto, auto c) {
                auto s = state(*services_, id);
                if (!s) {
                    return runtime::Size{};
                }
                s->measuring_tags = true;
                struct MeasureGuard {
                    S state;
                    ~MeasureGuard() {
                        state->measuring_tags = false;
                    }
                } guard{s};
                const auto revision = s->revision;
                const auto t = services_->components().theme_scope(id)->snapshot().cascader();
                const float width = std::max(0.F, c.max_width);
                const float gap = t.metrics.tag_gap;
                std::size_t limit = s->tags.size();
                bool responsive = false;
                if (s->props.max_tags_) {
                    const auto max = read_prop(*s->props.max_tags_);
                    if (const auto* count = std::get_if<std::size_t>(&max)) {
                        limit = std::min(limit, *count);
                    } else {
                        responsive = true;
                    }
                }
                auto owner = services_->components().parent(s->overflow).value();
                for (auto p = services_->components().parent(owner); p && *p != s->tag_root;
                     p = services_->components().parent(owner)) {
                    owner = *p;
                }
                std::vector<float> prefixes{0};
                for (const auto& tag : s->tags) {
                    tag->size = e.measure_child(services_->components().root(tag->component),
                                                {0, width, 0, std::numeric_limits<float>::infinity()});
                    prefixes.push_back(prefixes.back() + tag->size.width + (prefixes.size() > 1 ? gap : 0));
                }
                if (responsive && s->fitted_tags && s->fitted_width == width && s->fitted_prefixes == prefixes) {
                    limit = std::min(limit, *s->fitted_tags);
                }
                CascaderPaths omitted;
                runtime::Size overflow;
                // The actual placeholder participates in responsive fitting. Decrease only,
                // so even a non-monotonic custom label cannot cause a layout retry loop.
                while (true) {
                    omitted.clear();
                    for (std::size_t i = limit; i < s->tags.size(); ++i) {
                        omitted.push_back(s->tags[i]->path);
                    }
                    if (!omitted.empty()) {
                        invoke(*services_, id, [&] {
                            s->overflow_label.set(
                                s->props.tag_placeholder_
                                    ? s->props.tag_placeholder_(omitted)
                                    : String::from_utf8("+ " + std::to_string(omitted.size()) + " …").value());
                        });
                        if (!state(*services_, id)) {
                            return runtime::Size{};
                        }
                        if (s->revision != revision) {
                            services_->retire_failed_component(id);
                            return runtime::Size{};
                        }
                        // Popover's trigger host fills its constraint. Fit the actual
                        // summary button, then constrain that host to the measured width.
                        overflow = e.measure_child(services_->components().root(s->overflow),
                                                   {0, width, 0, std::numeric_limits<float>::infinity()});
                        (void)e.measure_child(services_->components().root(owner),
                                              {0, overflow.width, 0, std::numeric_limits<float>::infinity()});
                    } else {
                        (void)e.measure_child(services_->components().root(owner),
                                              {0, width, 0, std::numeric_limits<float>::infinity()});
                        overflow = {};
                    }
                    const float needed = prefixes[limit] + (omitted.empty() ? 0 : overflow.width + (limit ? gap : 0));
                    if (!responsive || limit == 0 || needed <= width) {
                        break;
                    }
                    --limit;
                }
                if (responsive) {
                    s->fitted_tags = limit;
                    s->fitted_width = width;
                    s->fitted_prefixes = prefixes;
                }
                float x = 0;
                float y = 0;
                float line = 0;
                float max_width = 0;
                const auto advance = [&](runtime::Size size) {
                    if (x > 0 && x + gap + size.width > width) {
                        max_width = std::max(max_width, x);
                        x = 0;
                        y += line + gap;
                        line = 0;
                    }
                    if (x > 0) {
                        x += gap;
                    }
                    x += size.width;
                    line = std::max(line, size.height);
                };
                for (std::size_t i = 0; i < s->tags.size(); ++i) {
                    auto& tag = s->tags[i];
                    tag->shown = i < limit;
                    services_->components().set_branch_active(tag->component, tag->shown);
                    services_->nodes().require(services_->components().root(tag->component)).out_of_flow = !tag->shown;
                    services_->components().set_branch_active(tag->overflow_item, !tag->shown);
                    services_->nodes().require(services_->components().root(tag->overflow_item)).out_of_flow =
                        tag->shown;
                    if (tag->shown) {
                        advance(tag->size);
                    }
                }
                const bool show = !omitted.empty();
                services_->components().set_branch_active(owner, show);
                services_->nodes().require(services_->components().root(owner)).out_of_flow = !show;
                s->overflow_disabled.set(!editable(id) || !show);
                if (show) {
                    advance(overflow);
                }
                s->tag_height = y + line;
                return c.constrain({std::min(width, std::max(max_width, x)), s->tag_height});
            },
            [this, id](auto& e, auto, auto rect) {
                auto s = state(*services_, id);
                if (!s) {
                    return;
                }
                const auto t = services_->components().theme_scope(id)->snapshot().cascader();
                float x = 0;
                float y = 0;
                float line = 0;
                const bool rtl = read_prop(s->props.direction_) == TextDirection::RightToLeft;
                const auto place = [&](runtime::ComponentId child, runtime::Size size) {
                    if (x > 0 && x + t.metrics.tag_gap + size.width > rect.width) {
                        x = 0;
                        y += line + t.metrics.tag_gap;
                        line = 0;
                    }
                    if (x > 0) {
                        x += t.metrics.tag_gap;
                    }
                    const float left = rtl ? rect.x + rect.width - x - size.width : rect.x + x;
                    e.place_child(services_->components().root(child),
                                  {left, rect.y + y, std::min(rect.width, size.width), size.height});
                    x += size.width;
                    line = std::max(line, size.height);
                };
                for (const auto& tag : s->tags) {
                    if (tag->shown) {
                        place(tag->component, tag->size);
                    }
                }
                auto owner = services_->components().parent(s->overflow).value();
                for (auto p = services_->components().parent(owner); p && *p != s->tag_root;
                     p = services_->components().parent(owner)) {
                    owner = *p;
                }
                if (services_->components().branch_active(owner)) {
                    place(owner, services_->nodes().require(services_->components().root(s->overflow)).measured_size);
                }
            }});
}

void CascaderComponentHost::reconcile_tags(runtime::ComponentId id) {
    auto s = state(*services_, id);
    if (!s || !s->tag_root.valid()) {
        return;
    }
    CascaderPaths paths = read_prop(s->props.multiple_) ? s->value : CascaderPaths{};
    if (read_prop(s->props.multiple_) && read_prop(s->props.strategy_) == CascaderCheckedStrategy::ShowChild) {
        paths.clear();
        for (const auto& r : s->rows) {
            if (r->option.children.empty() &&
                std::ranges::any_of(s->value, [&](auto& p) { return prefix(p, r->path); })) {
                paths.push_back(r->path);
            }
        }
        for (const auto& p : s->value) {
            if (std::ranges::none_of(s->rows, [&](auto& r) { return r->path == p; })) {
                paths.push_back(p);
            }
        }
    }
    const auto old = s->tags;
    const auto revision = s->revision;
    std::vector<std::shared_ptr<Tag>> next;
    const auto t = services_->components().theme_scope(id)->snapshot().cascader();
    try {
        for (const auto& path : paths) {
            const auto found = std::ranges::find_if(old, [&](auto& tag) { return tag->path == path; });
            auto tag = found == old.end() ? std::make_shared<Tag>() : *found;
            tag->path = path;
            const auto row = std::ranges::find_if(s->rows, [&](auto& r) { return r->path == path; });
            auto label = s->props.display_      ? s->props.display_({path})
                         : row == s->rows.end() ? raw_label(path)
                                                : join((*row)->ancestry);
            if (!state(*services_, id)) {
                return;
            }
            if (revision != s->revision) {
                services_->retire_failed_component(id);
                return;
            }
            if (s->props.max_tag_text_) {
                input::TextBoundaryMap map;
                (void)map.assign(label.bytes());
                const auto max = read_prop(*s->props.max_tag_text_);
                if (map.grapheme_count() > max) {
                    label = String::from_utf8(std::string(label.bytes().substr(0, map.grapheme_bytes()[max])) + "…")
                                .value();
                }
            }
            tag->label.set(label);
            tag->disabled.set(!editable(id) ||
                              (row != s->rows.end() && ((*row)->blocked || (*row)->option.disable_checkbox)));
            tag->theme.set(
                item_theme(sized_tags(t, read_prop(s->props.size_)), false, true,
                           std::ranges::find(services_->components().theme_scope(id)->snapshot().algorithms(),
                                             ThemeAlgorithm::Compact) !=
                               services_->components().theme_scope(id)->snapshot().algorithms().end()));
            if (found == old.end()) {
                const auto content = Content{[&, tag] {
                    auto& b = runtime::require_component_build_context();
                    tag->component = b.mount_component<int>(0);
                    const auto node = b.root(tag->component);
                    services_->nodes().require(node).clip_content = true;
                    services_->layout().set_layout(node, layout::BoxLayout{});
                    b.on_resource_cleanup(tag->component, [this, node] { services_->layout().remove_layout(node); });
                    b.mount_slot(
                        tag->component, Content{[&, tag] {
                            Theme(
                                ThemeProps{}.config(tag->theme), ThemeContent{[&, tag] {
                                    auto close = [this, weak = std::weak_ptr{s}, id, path = tag->path] {
                                        if (weak.lock() && state(*services_, id)) {
                                            remove_tag(id, path);
                                        }
                                    };
                                    if (s->props.tag_render_) {
                                        auto content =
                                            s->props.tag_render_({tag->path, tag->label, tag->disabled, close});
                                        SlotContentAccess::function(content)();
                                    } else {
                                        Button(
                                            ButtonProps{}.wave(false).disabled(tag->disabled),
                                            ButtonContent{[&, tag, close] {
                                                Flex(FlexProps{}.direction(s->flow).align(FlexAlign::Center).gap(dp(4)),
                                                     FlexContent{[&, tag, close] {
                                                         Text(TextProps{}.content(tag->label));
                                                         const auto before =
                                                             services_->interactions().declaration_order().size();
                                                         Button(ButtonProps{}
                                                                    .type(ButtonType::Text)
                                                                    .disabled(tag->disabled)
                                                                    .wave(false)
                                                                    .onClick(close),
                                                                ButtonContent{[&] {
                                                                    Icon(IconProps{}
                                                                             .source(s->props.remove_icon_)
                                                                             .twoToneColor(s->clear_color));
                                                                }});
                                                         tag->remove = services_->interactions()
                                                                           .require(services_->interactions()
                                                                                        .declaration_order()[before])
                                                                           .component;
                                                     }});
                                            }});
                                    }
                                }});
                        }});
                }};
                if (s->mounting) {
                    runtime::require_component_build_context().mount_slot(s->tag_root, content);
                } else {
                    services_->append_slot(s->tag_root, content);
                }
                const auto overflow = Content{[&, tag] {
                    auto& b = runtime::require_component_build_context();
                    tag->overflow_item = b.mount_component<int>(0);
                    const auto node = b.root(tag->overflow_item);
                    services_->layout().set_layout(node, layout::BoxLayout{});
                    b.on_resource_cleanup(tag->overflow_item,
                                          [this, node] { services_->layout().remove_layout(node); });
                    b.mount_slot(
                        tag->overflow_item, Content{[&, tag] {
                            Button(
                                ButtonProps{}.disabled(tag->disabled).wave(false).onClick([this, id, path = tag->path] {
                                    remove_tag(id, path);
                                }),
                                ButtonContent{[tag] { Text(TextProps{}.content(tag->label)); }});
                        }});
                }};
                if (s->mounting) {
                    runtime::require_component_build_context().mount_slot(s->overflow_content, overflow);
                } else {
                    services_->append_slot(s->overflow_content, overflow);
                }
            }
            if (!state(*services_, id)) {
                return;
            }
            if (revision != s->revision) {
                services_->retire_failed_component(id);
                return;
            }
            next.push_back(tag);
        }
    } catch (...) {
        services_->retire_failed_component(id);
        throw;
    }
    for (const auto& tag : old) {
        if (std::ranges::none_of(next, [&](auto& t) { return t == tag; })) {
            services_->destroy(tag->component);
            services_->destroy(tag->overflow_item);
        }
    }
    s->tags = std::move(next);
}

runtime::Size CascaderComponentHost::measure_panel(runtime::ComponentId id, layout::LayoutEngine& e,
                                                   layout::Constraints c) {
    auto s = state(*services_, id);
    if (!s) {
        return {};
    }
    const auto t = services_->components().theme_scope(id)->snapshot().cascader();
    std::size_t cols = 1;
    for (const auto& r : s->rows) {
        if (r->shown && s->effective_query.empty()) {
            cols = std::max(cols, r->column + 1);
        }
    }
    float width = s->column_width * static_cast<float>(cols);
    if (!s->effective_query.empty() && s->props.search_.match_input_width && s->selector.valid()) {
        width = std::max(width, services_->nodes().require(services_->components().root(s->selector)).bounds.width);
    }
    width = std::min(width, c.max_width);
    s->header_height = 0;
    if (read_prop(s->props.show_search_)) {
        s->header_height = e.measure_child(services_->components().root(s->search), {0, width, 0, s->height}).height +
                           t.metrics.menu_padding;
    }
    const bool status = s->loading.has_value() || !s->error.empty();
    s->status_height = status ? e.measure_child(services_->components().root(s->status),
                                                {0, width, 0, std::max(0.F, s->height - s->header_height)})
                                        .height +
                                    t.metrics.menu_padding
                              : 0;
    s->footer_height = s->footer.valid()
                           ? e.measure_child(services_->components().root(s->footer), {0, width, 0, s->height}).height
                           : 0;
    for (const auto& r : s->rows) {
        if (!r->shown) {
            continue;
        }
        (void)e.measure_child(services_->components().root(r->viewport),
                              {0, s->effective_query.empty() ? std::min(width, s->column_width) : width, 0,
                               std::numeric_limits<float>::infinity()});
        const auto size = services_->nodes().require(services_->components().root(r->component)).measured_size;
        r->row_height = std::max(t.metrics.option_height, (r->option.content ? size.height : t.typography.line_height) +
                                                              2 * t.metrics.option_padding_block);
    }
    s->empty_size = e.measure_child(services_->components().root(s->empty), {0, width, 0, s->height});
    return c.constrain({width, s->height});
}

void CascaderComponentHost::place_panel(runtime::ComponentId id, layout::LayoutEngine& e, runtime::Rect rect) {
    auto s = state(*services_, id);
    if (!s) {
        return;
    }
    const auto t = services_->components().theme_scope(id)->snapshot().cascader();
    s->panel_bounds = rect;
    const float search_header = std::min(rect.height, s->header_height);
    const float status_height = std::min(std::max(0.F, rect.height - search_header), s->status_height);
    const float header = search_header + status_height;
    const float footer = std::min(std::max(0.F, rect.height - header), s->footer_height);
    const float height = std::max(0.F, rect.height - header - footer - 2 * t.metrics.menu_padding);
    s->viewport_height = height;
    if (read_prop(s->props.show_search_)) {
        e.place_child(services_->components().root(s->search),
                      {rect.x, rect.y, rect.width, std::max(0.F, search_header - t.metrics.menu_padding)}, true, true);
    }
    if (s->status_height > 0) {
        e.place_child(
            services_->components().root(s->status),
            {rect.x, rect.y + search_header, rect.width, std::max(0.F, status_height - t.metrics.menu_padding)}, true,
            true);
    }
    if (s->footer.valid()) {
        e.place_child(services_->components().root(s->footer),
                      {rect.x, rect.y + rect.height - footer, rect.width, footer});
    }
    e.place_child(services_->components().root(s->empty),
                  {rect.x, rect.y + header, rect.width, std::min(height, s->empty_size.height)});
    s->scroll.resize(34);
    s->extents.assign(34, 0);
    std::vector<std::shared_ptr<Row>> rows;
    for (const auto& r : s->rows) {
        if (r->shown) {
            rows.push_back(r);
        }
    }
    std::stable_sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a->order < b->order; });
    for (const auto& r : rows) {
        const auto col = s->effective_query.empty() ? r->column : 0;
        r->row_y = s->extents[col];
        s->extents[col] += r->row_height;
    }
    const auto focused = services_->focus().state().focused;
    const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
    for (const auto& r : rows) {
        if ((s->reveal && *s->reveal == r->option.key) ||
            (!s->wheel_scrolling && target && descendant(*services_, target->component, r->component))) {
            const auto col = s->effective_query.empty() ? r->column : 0;
            if (r->row_y < s->scroll[col]) {
                s->scroll[col] = r->row_y;
            } else if (r->row_y + r->row_height > s->scroll[col] + height) {
                s->scroll[col] = r->row_y + r->row_height - height;
            }
        }
    }
    s->reveal.reset();
    for (std::size_t i = 0; i < s->scroll.size(); ++i) {
        s->scroll[i] = std::clamp(s->scroll[i], 0.F, std::max(0.F, s->extents[i] - height));
    }
    const float col_width = std::min(s->column_width, rect.width);
    std::size_t columns = 1;
    for (const auto& r : rows) {
        if (s->effective_query.empty()) {
            columns = std::max(columns, r->column + 1);
        }
    }
    std::optional<std::size_t> reveal_column;
    if (s->reveal_columns) {
        reveal_column = columns - 1;
    }
    if (target && !s->wheel_scrolling) {
        for (const auto& r : rows) {
            if (descendant(*services_, target->component, r->component)) {
                reveal_column = s->effective_query.empty() ? r->column : 0;
                break;
            }
        }
    }
    if (reveal_column) {
        const float start = col_width * static_cast<float>(*reveal_column);
        if (start < s->horizontal_scroll) {
            s->horizontal_scroll = start;
        } else if (start + col_width > s->horizontal_scroll + rect.width) {
            s->horizontal_scroll = start + col_width - rect.width;
        }
    }
    s->reveal_columns = false;
    s->horizontal_scroll =
        std::clamp(s->horizontal_scroll, 0.F, std::max(0.F, col_width * static_cast<float>(columns) - rect.width));

    for (const auto& r : rows) {
        const auto col = s->effective_query.empty() ? r->column : 0;
        const float width = s->effective_query.empty() ? col_width : rect.width;
        const float x = read_prop(s->props.direction_) == TextDirection::RightToLeft
                            ? rect.x + rect.width - col_width * static_cast<float>(col) - width + s->horizontal_scroll
                            : rect.x + col_width * static_cast<float>(col) - s->horizontal_scroll;
        r->viewport_bounds = {x, rect.y + header + t.metrics.menu_padding, width, height};
        r->row_y += r->viewport_bounds.y - s->scroll[col];
        e.place_child(services_->components().root(r->viewport), r->viewport_bounds, true, true);
    }
    // A rich child's position is known after its layout. Reveal that exact editor,
    // including when a retained slot grows taller than a column in this pass.
    if (target && !s->wheel_scrolling && height > 0) {
        for (const auto& focused_row : rows) {
            if (target->component == focused_row->component ||
                !descendant(*services_, target->component, focused_row->component)) {
                continue;
            }
            bool child_layer = false;
            for (auto p = std::optional{target->component}; p && *p != focused_row->component;
                 p = services_->components().parent(*p)) {
                if (services_->nodes().require(services_->components().root(*p)).clip_boundary) {
                    child_layer = true;
                    break;
                }
            }
            if (child_layer) {
                break;
            }
            const auto target_bounds =
                services_->nodes().require(services_->components().root(target->component)).bounds;
            const auto viewport = focused_row->viewport_bounds;
            float shift = 0;
            if (target_bounds.y < viewport.y || target_bounds.height > height) {
                shift = target_bounds.y - viewport.y;
            } else if (target_bounds.y + target_bounds.height > viewport.y + height) {
                shift = target_bounds.y + target_bounds.height - viewport.y - height;
            }
            const auto col = s->effective_query.empty() ? focused_row->column : 0;
            const float next = std::clamp(s->scroll[col] + shift, 0.F, std::max(0.F, s->extents[col] - height));
            const float delta = next - s->scroll[col];
            s->scroll[col] = next;
            if (delta != 0) {
                for (const auto& r : rows) {
                    if ((s->effective_query.empty() ? r->column : 0) == col) {
                        r->row_y -= delta;
                        e.place_child(services_->components().root(r->viewport), r->viewport_bounds, true, true);
                    }
                }
            }
            break;
        }
    }
}

void CascaderComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (auto id : std::vector(mounted_)) {
        auto s = state(*services_, id);
        if (!s) {
            continue;
        }
        const auto& node = services_->nodes().require(s->node);
        const auto content = services_->component_clip(id, viewport, clip);
        const bool visible = node.bounds.width > 0 && node.bounds.height > 0 && content.width > 0 && content.height > 0;
        if (s->geometry_available != visible) {
            s->geometry_available = visible;
            refresh(id);
        }
        if (!state(*services_, id)) {
            continue;
        }
        const auto& pn = services_->nodes().require(services_->components().root(s->panel));
        const auto pc = services_->component_clip(s->panel, viewport, clip);
        const auto t = services_->components().theme_scope(id)->snapshot().cascader();
        std::vector<graphics::QuadInstance> quads;
        const auto add = [&](runtime::Rect bounds, Color color) {
            bounds.x += pn.translation.x;
            bounds.y += pn.translation.y;
            bounds = graphics::intersect_effect_bounds(bounds, pc);
            if (bounds.width > 0 && bounds.height > 0) {
                quads.push_back({{bounds.x, bounds.y, bounds.width, bounds.height},
                                 {color.red(), color.green(), color.blue(), color.alpha()},
                                 services_->nodes().content_opacity(services_->components().root(s->panel))});
            }
        };
        if (services_->components().branch_active(s->panel)) {
            add(s->panel_bounds, t.colors.popup_background);
            if (s->effective_query.empty()) {
                std::vector<std::size_t> columns;
                for (auto& r : s->rows) {
                    if (r->shown && r->column > 0 && std::ranges::find(columns, r->column) == columns.end()) {
                        columns.push_back(r->column);
                        add({r->viewport_bounds.x, r->viewport_bounds.y, 1, r->viewport_bounds.height},
                            t.colors.separator);
                    }
                }
            }
        }
        (void)services_->surfaces().update_content_range(s->background, quads);
    }
}

void CascaderComponentHost::on_focus_changed() {
    prepare_auxiliary_layout();
    for (auto id : std::vector(mounted_)) {
        auto s = state(*services_, id);
        if (!s || s->mounting || s->reconciling || s->refreshing) {
            continue;
        }
        const auto focused = services_->focus().state().focused;
        const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
        const bool inside = target && descendant(*services_, target->component, id);
        if (s->props.trigger_) {
            const bool selector_focused = target && target->component == s->selector;
            if (selector_focused != s->selector_focused) {
                s->selector_focused = selector_focused;
                emit_focus(id, selector_focused);
                if (!state(*services_, id)) {
                    continue;
                }
            }
        }
        const bool departed = s->focus_inside && !inside;
        s->focus_inside = inside;
        if (departed) {
            services_->popover().dismiss_descendants(id);
            if (!state(*services_, id)) {
                continue;
            }
        }
        if (inside) {
            s->wheel_scrolling = false;
            for (auto& r : s->rows) {
                if (descendant(*services_, target->component, r->component)) {
                    s->reveal = r->option.key;
                    services_->dirty().invalidate(services_->components().root(s->panel),
                                                  runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
                    break;
                }
            }
        }
    }
}

void CascaderComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                             std::optional<input::InteractionId> hit,
                                             std::optional<input::InteractionId>) {
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    if (!target) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto s = state(*services_, id);
        if (!s || !editable(id)) {
            continue;
        }
        if (event.action == input::PointerAction::down && target->component == s->selector && !s->props.trigger_) {
            if (read_prop(s->props.show_search_)) {
                if (!s->open) {
                    request_open(id, true);
                }
            } else {
                request_open(id, !s->open);
            }
            return;
        }
        if (event.action == input::PointerAction::move &&
            read_prop(s->props.expand_trigger_) == CascaderExpandTrigger::Hover) {
            if (services_->popover().owns_escape(s->panel, target->component)) {
                continue;
            }
            for (const auto& r : s->rows) {
                if (descendant(*services_, target->component, r->component) && r->shown && !r->blocked &&
                    s->expanded != r->path) {
                    activate(id, r->option.key, false);
                    return;
                }
            }
        }
    }
}

bool CascaderComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (mounted_.empty() || !services_->input_runtime() || event.action != input::KeyAction::down ||
        event.modifiers != input::KeyModifier::none) {
        return false;
    }
    const auto focused = services_->focus().state().focused;
    const auto* target = focused ? services_->interactions().find(*focused) : nullptr;
    if (!target) {
        return false;
    }
    // Core composition owns candidate navigation and Escape until cancelled.
    for (const auto& input : services_->input_runtime()->mounted_inputs()) {
        if (input.component == target->component) {
            const auto* editor = services_->input_runtime()->editors().find(input.editor);
            if (editor && editor->composition().active) {
                return false;
            }
        }
    }
    for (auto id : std::vector(mounted_)) {
        auto s = state(*services_, id);
        if (!s || !editable(id) || !descendant(*services_, target->component, id)) {
            continue;
        }
        using input::Key;
        if (event.key == Key::escape && s->open) {
            if (services_->popover().owns_escape(s->panel, target->component) ||
                (s->tag_root.valid() && services_->popover().owns_escape(s->tag_root, target->component))) {
                return false;
            }
            request_open(id, false);
            if (state(*services_, id)) {
                (void)s->props.reference_.focus();
            }
            return true;
        }
        auto it = std::ranges::find_if(s->rows, [&](auto& r) { return r->component == target->component; });
        const bool selector = target->component == s->selector;
        const bool search = target->component == s->search;
        const bool selector_search = selector && read_prop(s->props.show_search_);
        if (selector_search && event.key == Key::enter && !s->effective_query.empty()) {
            std::shared_ptr<Row> match;
            for (const auto& row : s->rows) {
                if (row->shown && !row->disabled.get() && (!match || row->order < match->order)) {
                    match = row;
                }
            }
            if (match) {
                activate(id, match->option.key, true);
            }
            return true;
        }
        const bool opening = selector && (event.key == Key::down || event.key == Key::up || event.key == Key::enter ||
                                          (!selector_search && event.key == Key::space));
        if (opening) {
            request_open(id, true);
            if (!state(*services_, id)) {
                return true;
            }
        }
        if (it == s->rows.end() && !opening &&
            !(search && (event.key == Key::down || event.key == Key::up || event.key == Key::page_down ||
                         event.key == Key::page_up))) {
            return false;
        }
        const auto current = it == s->rows.end() ? std::shared_ptr<Row>{} : *it;
        const bool rtl = read_prop(s->props.direction_) == TextDirection::RightToLeft;
        const bool forward = event.key == (rtl ? Key::left : Key::right);
        const bool back = event.key == (rtl ? Key::right : Key::left);
        if (current && forward) {
            activate(id, current->option.key, false);
            if (!state(*services_, id)) {
                return true;
            }
        }
        const std::size_t column = !s->effective_query.empty() ? 0
                                   : current                   ? (forward                   ? current->column + 1
                                                                  : back && current->column ? current->column - 1
                                                                                            : current->column)
                                                               : 0;
        std::vector<std::shared_ptr<Row>> eligible;
        for (const auto& r : s->rows) {
            if (r->shown && !r->disabled.get() && (s->effective_query.empty() ? r->column : 0) == column) {
                eligible.push_back(r);
            }
        }
        std::stable_sort(eligible.begin(), eligible.end(), [](auto& a, auto& b) { return a->order < b->order; });
        if (eligible.empty()) {
            return forward || back || opening;
        }
        auto found = std::ranges::find(eligible, current);
        std::size_t index = found == eligible.end() ? 0 : static_cast<std::size_t>(found - eligible.begin());
        if (back && current && current->path.size() > 1) {
            auto path = current->path;
            path.pop_back();
            const auto parent = std::ranges::find_if(eligible, [&](auto& r) { return r->path == path; });
            if (parent != eligible.end()) {
                index = static_cast<std::size_t>(parent - eligible.begin());
            }
        } else if (event.key == Key::down && !opening) {
            index = current ? (index + 1) % eligible.size() : 0;
        } else if (event.key == Key::up) {
            index = current ? (index + eligible.size() - 1) % eligible.size() : eligible.size() - 1;
        } else if (event.key == Key::home) {
            index = 0;
        } else if (event.key == Key::end) {
            index = eligible.size() - 1;
        } else if (event.key == Key::page_down || event.key == Key::page_up) {
            float moved = 0;
            while (moved < s->viewport_height && ((event.key == Key::page_down && index + 1 < eligible.size()) ||
                                                  (event.key == Key::page_up && index > 0))) {
                moved += eligible[index]->row_height;
                if (event.key == Key::page_down) {
                    ++index;
                } else {
                    --index;
                }
            }
        } else if (!forward && !back && !opening) {
            return false;
        }
        s->wheel_scrolling = false;
        s->reveal = eligible[index]->option.key;
        services_->dirty().invalidate(services_->components().root(s->panel),
                                      runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
        (void)eligible[index]->ref.focus();
        return true;
    }
    return false;
}

bool CascaderComponentHost::on_scroll_input(const input::ScrollInputEvent& event) {
    if (!std::isfinite(event.delta_y) || !std::isfinite(event.x) || !std::isfinite(event.y) || event.delta_y == 0) {
        return false;
    }
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* target = hit ? services_->interactions().find(*hit) : nullptr;
    const auto layer = [&](runtime::ComponentId id) {
        for (auto p = std::optional{id}; p && services_->components().contains(*p);
             p = services_->components().parent(*p)) {
            if (services_->nodes().require(services_->components().root(*p)).clip_boundary) {
                return *p;
            }
        }
        return runtime::ComponentId{};
    };
    for (auto id : std::vector(mounted_)) {
        auto s = state(*services_, id);
        if (!s || !editable(id) || (!s->embedded && !s->popup_open.get())) {
            continue;
        }
        if (target && layer(target->component) != layer(s->panel)) {
            continue;
        }
        for (const auto& r : s->rows) {
            const auto& n = services_->nodes().require(services_->components().root(r->viewport));
            auto rect = r->viewport_bounds;
            rect.x += n.translation.x;
            rect.y += n.translation.y;
            const auto clip = services_->nodes().content_clip(services_->components().root(r->viewport), rect);
            if (!r->shown || !contains(clip, event.x, event.y)) {
                continue;
            }
            const auto col = s->effective_query.empty() ? r->column : 0;
            const float maximum = std::max(0.F, s->extents[col] - s->viewport_height);
            const auto next = std::clamp(s->scroll[col] - event.delta_y * 96, 0.F, maximum);
            if (next == s->scroll[col]) {
                return false;
            }
            s->scroll[col] = next;
            // Wheel movement takes authority until focus actually moves again.
            s->reveal.reset();
            s->wheel_scrolling = true;
            services_->dirty().invalidate(services_->components().root(s->panel),
                                          runtime::DirtyFlags::Layout | runtime::DirtyFlags::HitTest);
            return true;
        }
    }
    return false;
}
} // namespace ryn::detail

namespace ryn {
void Cascader(CascaderProps props) {
    if (!detail::active_cascader) {
        throw std::logic_error("Cascader requires WindowComponentServices");
    }
    detail::active_cascader->mount(props);
}

void CascaderPanel(CascaderProps props) {
    if (!detail::active_cascader) {
        throw std::logic_error("CascaderPanel requires WindowComponentServices");
    }
    detail::active_cascader->mount(props, true);
}
} // namespace ryn
