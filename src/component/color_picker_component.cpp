#include "component/color_picker_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/localized_prop.hpp"
#include "i18n/number_format.hpp"
#include "graphics/color_field.hpp"
#include "runtime/prop_connection.hpp"
#include "runtime/layout_style_adapter.hpp"
#include <ryn/flex.hpp>
#include <ryn/input_number.hpp>
#include <ryn/text.hpp>
#include <algorithm>
#include <cmath>
#include <charconv>

namespace ryn::detail {
namespace {
thread_local ColorPickerComponentHost* active_picker{};

String display_label(const ColorPickerValue& value, ColorFormat format, const StatisticLocale& locale) {
    if (format != ColorFormat::Hsb || !value.solid()) {
        return value.format(format);
    }
    const auto number = [&locale](double value) {
        std::array<char, 64> buffer;
        const auto result =
            std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, 2);
        std::string text(buffer.data(), result.ptr);
        while (text.back() == '0') {
            text.pop_back();
        }
        if (text.back() == '.') {
            text.pop_back();
        }
        NumberFormatOptions options;
        options.grouping = false;
        return std::string(format_decimal_text(String::from_utf8(text).value(), locale, options).bytes());
    };
    const auto hsb = value.solid()->hsb();
    const bool alpha = hsb.a < 1;
    return String::from_utf8(std::string{alpha ? "hsba(" : "hsb("} + number(hsb.h) + ", " + number(hsb.s * 100) +
                             "%, " + number(hsb.b * 100) + "%" + (alpha ? ", " + number(hsb.a) : "") + ")")
        .value();
}

struct Field final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    runtime::Rect bounds;
    bool focus{};
    component::RetainedSurfaceId hit_surface;
};

struct PickerState final {
    runtime::NodeId node;
    runtime::ComponentId panel;
    runtime::ComponentId presets_root;
    runtime::ComponentId swatch;
    std::optional<Field> preview;
    std::vector<Field> fields;
    Signal<ColorPickerValue> value{ColorPickerValue{}};
    ColorPickerValue candidate;
    ColorPickerValue gesture_start;
    std::optional<ColorPickerValue> cached_gradient;
    std::shared_ptr<bool> render_alive{std::make_shared<bool>(true)};
    Signal<ColorFormat> format{ColorFormat::Hex};
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    Signal<bool> alpha_disabled{false};
    Signal<bool> format_disabled{false};
    Signal<bool> clear{false};
    Signal<bool> show_text{false};
    Signal<String> text{String{}};
    Signal<bool> controls_disabled{false};
    Signal<bool> alpha_input_disabled{false};
    Signal<bool> clear_disabled{true};
    Signal<bool> mode_disabled{true};
    Signal<bool> stop_disabled{true};
    Signal<bool> remove_disabled{true};
    Signal<ColorPickerLocale> locale{ColorPickerLocale{}};
    std::array<Signal<InputNumberValue>, 4> channels{Signal<InputNumberValue>{{}}, Signal<InputNumberValue>{{}},
                                                     Signal<InputNumberValue>{{}}, Signal<InputNumberValue>{{}}};
    Signal<InputNumberValue> stop_percent{InputNumberValue{0}};
    Signal<double> channel_max{255};
    std::array<Signal<String>, 4> labels{Signal<String>{String{u8"R"}}, Signal<String>{String{u8"G"}},
                                         Signal<String>{String{u8"B"}}, Signal<String>{String{u8"A"}}};
    std::vector<ColorPickerMode> modes;
    std::vector<ColorPreset> presets;
    std::vector<std::pair<ColorPreset, runtime::ComponentId>> preset_groups;
    bool presets_dirty{};
    std::vector<std::pair<runtime::ComponentId, Signal<bool>>> expanded;
    String selected;
    std::uint64_t next_stop{1};
    bool rtl{};
    bool controlled{};
    bool format_controlled{};
    bool open_controlled{};
    bool gesture{};
    bool publishing{};
    bool refreshing{};
    bool ready{};
    std::optional<input::PointerIdentity> pointer;
    std::size_t active_field{};
    std::function<void(ColorPickerValue, String)> change;
    std::function<void(ColorPickerValue)> complete;
    std::function<void()> on_clear;
    std::function<void(ColorFormat)> format_change;
    std::function<void(bool)> open_change;
    std::array<theme_runtime::Subscription, 2> theme;
};

PickerColor selected_color(const PickerState& s) {
    const auto& value = s.gesture ? s.candidate : s.value.get();
    if (value.gradient()) {
        const auto stops = value.stops();
        const auto found = std::ranges::find(stops, s.selected, &ColorStop::key);
        return found == stops.end() ? stops.front().color : found->color;
    }
    return value.solid().value_or(PickerColor{});
}

ColorPickerValue replace_color(const PickerState& s, PickerColor color) {
    const auto value = s.gesture ? s.candidate : s.value.get();
    if (!value.gradient()) {
        if (s.modes == std::vector<ColorPickerMode>{ColorPickerMode::Gradient}) {
            return ColorPickerValue{
                std::vector<ColorStop>{{String{u8"start"}, color, 0}, {String{u8"end"}, color, 100}}};
        }
        return ColorPickerValue{color};
    }
    std::vector<ColorStop> stops{value.stops().begin(), value.stops().end()};
    auto found = std::ranges::find(stops, s.selected, &ColorStop::key);
    (found == stops.end() ? stops.front() : *found).color = color;
    return ColorPickerValue{std::move(stops)};
}

bool available(WindowComponentServices& h, runtime::ComponentId id, const PickerState& s) {
    return !s.disabled.get() && h.components().branch_active(id) && h.interactions().component_active(id) &&
           h.focus().state().window_active;
}

Color composite(Color source, Color back) {
    const auto a = source.alpha();
    return {source.red() * a + back.red() * (1 - a), source.green() * a + back.green() * (1 - a),
            source.blue() * a + back.blue() * (1 - a), 1};
}
} // namespace

ColorPickerComponentHost::ColorPickerComponentHost(WindowComponentServices& s) : services_(&s) {
    s.attach(*this);
}

ColorPickerComponentHost::~ColorPickerComponentHost() {
    services_->detach(*this);
}

void* ColorPickerComponentHost::begin_mount() noexcept {
    return std::exchange(active_picker, this);
}

void ColorPickerComponentHost::end_mount(void* p) noexcept {
    active_picker = static_cast<ColorPickerComponentHost*>(p);
}

void ColorPickerComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

ColorPickerSnapshot ColorPickerComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<PickerState>(id);
    if (!s) {
        throw std::out_of_range("ColorPicker is stale");
    }
    ColorPickerSnapshot v{s->value.get(), s->format.get(), s->open.get(), s->gesture, s->panel, {}, {}};
    for (const auto& f : s->fields) {
        v.fields.push_back(f.component);
        v.interactions.push_back(f.interaction);
    }
    return v;
}

void ColorPickerComponentHost::cancel(runtime::ComponentId id) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s) {
        return;
    }
    s->pointer.reset();
    s->gesture = false;
    const auto fields = s->fields;
    for (const auto& f : fields) {
        services_->pointer().cancel_interaction(f.interaction);
    }
}

void ColorPickerComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || s->refreshing) {
        return;
    }
    s->refreshing = true;
    const auto value = s->value.get();
    const auto disabled = s->disabled.get();
    services_->set_input_blocked(id, disabled);
    s = services_->components().state<PickerState>(id);
    if (!s) {
        return;
    }
    s->controls_disabled.set(disabled);
    s->alpha_input_disabled.set(disabled || s->alpha_disabled.get());
    s->clear_disabled.set(disabled || !s->clear.get() || value.empty());
    s->mode_disabled.set(disabled || s->modes.size() < 2);
    s->stop_disabled.set(disabled || !value.gradient());
    s->remove_disabled.set(disabled || !value.gradient() || value.stops().size() <= 2);
    const auto color = selected_color(*s);
    const auto rgb = color.rgb();
    const auto hsb = color.hsb();
    const bool h = s->format.get() == ColorFormat::Hsb;
    const auto channel_value = [](double v) {
        return std::round(v * 100) / 100;
    };
    s->channels[0].set(channel_value(h ? hsb.h : rgb.r));
    s->channels[1].set(channel_value(h ? hsb.s * 100 : rgb.g));
    s->channels[2].set(channel_value(h ? hsb.b * 100 : rgb.b));
    s->channels[3].set(channel_value(rgb.a * 100));
    s->channel_max.set(h ? 360 : 255);
    s->labels[0].set(h ? String{u8"H"} : String{u8"R"});
    s->labels[1].set(h ? String{u8"S %"} : String{u8"G"});
    s->labels[2].set(h ? String{u8"B %"} : String{u8"B"});
    s->text.set(value.empty() ? String{} : color.format(s->format.get(), !s->alpha_disabled.get()));
    if (value.gradient()) {
        if (std::ranges::find(value.stops(), s->selected, &ColorStop::key) == value.stops().end()) {
            s->selected = value.stops().front().key;
        }
        const auto selected = std::ranges::find(value.stops(), s->selected, &ColorStop::key);
        s->stop_percent.set(channel_value(selected->percent));
    }
    const bool usable = available(*services_, id, *s) && s->open.get();
    for (std::size_t i = 0; i < s->fields.size(); ++i) {
        auto& f = s->fields[i];
        const bool enabled = usable && (i != 2 || !s->alpha_disabled.get()) && (i != 3 || value.gradient());
        services_->interactions().set_eligible(f.interaction, enabled);
    }
    s->refreshing = false;
    if (!usable) {
        cancel(id);
    }
    if (auto* current = services_->components().state<PickerState>(id)) {
        services_->dirty().invalidate(current->node, runtime::DirtyFlags::Material | runtime::DirtyFlags::Geometry |
                                                         runtime::DirtyFlags::HitTest);
    }
}

void ColorPickerComponentHost::publish(runtime::ComponentId id, ColorPickerValue value, bool finished) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !available(*services_, id, *s)) {
        return;
    }
    if (s->alpha_disabled.get() && !value.empty()) {
        const auto opaque = [](PickerColor color) {
            if (color.rgb().a == 1) {
                return color;
            }
            auto hsb = color.hsb();
            hsb.a = 1;
            return PickerColor{hsb};
        };
        if (value.gradient()) {
            std::vector<ColorStop> stops{value.stops().begin(), value.stops().end()};
            for (auto& stop : stops) {
                stop.color = opaque(stop.color);
            }
            value = ColorPickerValue{std::move(stops)};
        } else {
            value = ColorPickerValue{opaque(*value.solid())};
        }
    }
    s->cached_gradient.reset();
    const auto before = s->gesture ? s->candidate : s->value.get();
    const bool changed = before != value;
    s->candidate = value;
    const auto change = s->change;
    const auto css = value.format(ColorFormat::Rgb);
    if (!s->controlled) {
        s->value.set(value);
    }
    refresh(id);
    try {
        if (changed && change) {
            change(value, css);
        }
    } catch (...) {
        cancel(id);
        throw;
    }
    if (finished && services_->components().contains(id)) {
        s = services_->components().state<PickerState>(id);
        const auto callback = s->complete;
        if (callback && changed) {
            callback(value);
        }
    }
}

void ColorPickerComponentHost::complete(runtime::ComponentId id) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !s->gesture) {
        return;
    }
    const auto value = s->candidate;
    const bool changed = value != s->gesture_start;
    const auto callback = s->complete;
    s->gesture = false;
    s->pointer.reset();
    refresh(id);
    if (changed && callback) {
        callback(value);
    }
}

void ColorPickerComponentHost::channel(runtime::ComponentId id, std::size_t index, double v, bool done) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || s->refreshing || !std::isfinite(v) || !available(*services_, id, *s) ||
        (index == 3 && s->alpha_disabled.get())) {
        return;
    }
    auto color = selected_color(*s);
    if (s->format.get() == ColorFormat::Hsb) {
        auto h = color.hsb();
        if (index == 0) {
            h.h = std::clamp(v, 0.0, 360.0);
        } else if (index == 1) {
            h.s = std::clamp(v / 100, 0.0, 1.0);
        } else if (index == 2) {
            h.b = std::clamp(v / 100, 0.0, 1.0);
        } else {
            h.a = std::clamp(v / 100, 0.0, 1.0);
        }
        color = PickerColor{h};
    } else {
        auto rgb = color.rgb();
        if (index == 0) {
            rgb.r = std::clamp(v, 0.0, 255.0);
        } else if (index == 1) {
            rgb.g = std::clamp(v, 0.0, 255.0);
        } else if (index == 2) {
            rgb.b = std::clamp(v, 0.0, 255.0);
        } else {
            rgb.a = std::clamp(v / 100, 0.0, 1.0);
        }
        color = PickerColor{rgb};
    }
    publish(id, replace_color(*s, color), done);
}

void ColorPickerComponentHost::move_stop(runtime::ComponentId id, double percent, bool done) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !std::isfinite(percent)) {
        return;
    }
    const auto value = s->gesture ? s->candidate : s->value.get();
    if (!value.gradient()) {
        return;
    }
    std::vector<ColorStop> stops{value.stops().begin(), value.stops().end()};
    auto selected = std::ranges::find(stops, s->selected, &ColorStop::key);
    if (selected == stops.end()) {
        return;
    }
    selected->percent = std::clamp(percent, 0.0, 100.0);
    publish(id, ColorPickerValue{std::move(stops)}, done);
}

void ColorPickerComponentHost::mount_picker(runtime::ComponentId id) {
    FormPrivateEditorScope private_editor;
    auto& h = *services_;
    auto& build = runtime::require_component_build_context();
    auto& s = *h.components().state<PickerState>(id);
    Flex(FlexProps{}.vertical(true).gap(SpaceSize::Small), [this, id] {
        auto& h = *services_;
        auto& b = runtime::require_component_build_context();
        auto& s = *h.components().state<PickerState>(id);
        s.panel = b.mount_component<int>(0);
        h.layout().set_layout(b.root(s.panel), layout::FlexLayout{.direction = layout::FlexDirection::vertical});
        b.on_resource_cleanup(s.panel, [&h, node = b.root(s.panel)] { h.layout().remove_layout(node); });
        b.mount_slot(
            s.panel, Content{[this, id] {
                auto& b = runtime::require_component_build_context();
                auto& h = *services_;
                auto& s = *h.components().state<PickerState>(id);
                for (std::size_t i = 0; i < 4; ++i) {
                    Field f;
                    f.component = b.mount_component<int>(0);
                    f.node = b.root(f.component);
                    f.interaction = h.interactions().create({f.component, f.node, {}, true, true, {}});
                    auto fragment =
                        b.register_scene_fragment(f.component, runtime::SceneFragmentPlacement::before_children);
                    f.surface = h.surfaces().create_content_range(fragment, {});
                    const auto hit_fragment =
                        b.register_scene_fragment(f.component, runtime::SceneFragmentPlacement::after_children);
                    graphics::QuadInstance hit_quad;
                    hit_quad.opacity = 0;
                    const std::array hit_quads{hit_quad};
                    f.hit_surface = h.surfaces().create(f.component, f.node, hit_fragment, f.interaction, hit_quads);
                    h.layout().set_layout(
                        f.node, layout::ComponentLayout{
                                    [this, id, i](auto&, auto, auto limits) {
                                        const auto& t =
                                            services_->components().theme_scope(id)->snapshot().color_picker();
                                        return limits.constrain(
                                            {t.panel_width, i == 0 ? t.saturation_height : t.rail_height * 2 + t.gap});
                                    },
                                    [this, id, i](auto&, auto, auto bounds) {
                                        if (auto* state = services_->components().state<PickerState>(id);
                                            state && i < state->fields.size()) {
                                            state->fields[i].bounds = bounds;
                                        }
                                    }});
                    b.on_resource_cleanup(f.component, [this, f] {
                        auto& h = *services_;
                        h.pointer().cancel_interaction(f.interaction);
                        h.focus().cancel_interaction(f.interaction);
                        h.interactions().remove(f.interaction);
                        h.surfaces().destroy_content_range(f.surface);
                        h.surfaces().destroy(f.hit_surface);
                        h.layout().remove_layout(f.node);
                    });
                    input::InteractionHandlers handlers;
                    handlers.target = [this, id, i](auto& event) {
                        pointer(id, i, event);
                    };
                    h.interactions().set_handlers(f.interaction, std::move(handlers));
                    input::FocusHandlers focus;
                    focus.text_edit = [this, id, i](auto& event) {
                        return keyboard(id, i, event);
                    };
                    focus.state_changed = [this, id, i](auto value) {
                        if (auto* state = services_->components().state<PickerState>(id)) {
                            state->fields[i].focus = value.focus_visible;
                            if (!value.focused && state->gesture && !state->pointer) {
                                cancel(id);
                            }
                            refresh(id);
                        }
                    };
                    h.interactions().set_focus_handlers(f.interaction, std::move(focus));
                    s.fields.push_back(f);
                }
            }});
        Input(InputProps{}
                  .value(s.text)
                  .disabled(s.controls_disabled)
                  .onChange([this, id](String text) {
                      if (auto color = PickerColor::parse(text.bytes())) {
                          auto* s = services_->components().state<PickerState>(id);
                          if (!s) {
                              return;
                          }
                          publish(id, replace_color(*s, *color), true);
                      }
                  })
                  .onSubmit([this, id](String) { refresh(id); })
                  .onBlur([this, id] { refresh(id); })
                  .layout(LayoutStyle{}.width(dp(234))));
        Flex(FlexProps{}.gap(SpaceSize::Small), [this, id] {
            auto& s = *services_->components().state<PickerState>(id);
            for (std::size_t i = 0; i < 4; ++i) {
                Flex(FlexProps{}.vertical(true), [this, id, i] {
                    auto& s = *services_->components().state<PickerState>(id);
                    Text(TextProps{}.content(s.labels[i]));
                    auto max = bind([f = s.format, i] {
                        return i == 3 ? 100.0 : f.get() == ColorFormat::Hsb ? (i == 0 ? 360.0 : 100.0) : 255.0;
                    });
                    InputNumber(InputNumberProps{}
                                    .value(s.channels[i])
                                    .min(0)
                                    .max(max)
                                    .step(i == 3 ? .1 : 1)
                                    .precision(2)
                                    .size(ControlSize::Small)
                                    .controls(false)
                                    .disabled(i == 3 ? s.alpha_input_disabled : s.controls_disabled)
                                    .onChange([this, id, i](InputNumberValue v) {
                                        if (v) {
                                            channel(id, i, *v, true);
                                        }
                                    })
                                    .layout(LayoutStyle{}.width(dp(72))));
                });
            }
        });
        Flex(FlexProps{}.gap(SpaceSize::Small), [this, id] {
            auto& s = *services_->components().state<PickerState>(id);
            Button(ButtonProps{}
                       .disabled(bind([d = s.disabled, f = s.format_disabled] { return d.get() || f.get(); }))
                       .onClick([this, id] {
                           auto* s = services_->components().state<PickerState>(id);
                           if (!s) {
                               return;
                           }
                           const auto next = static_cast<ColorFormat>((static_cast<int>(s->format.get()) + 1) % 3);
                           const auto callback = s->format_change;
                           if (!s->format_controlled) {
                               s->format.set(next);
                           }
                           refresh(id);
                           if (callback) {
                               callback(next);
                           }
                       }),
                   [] { Text(TextProps{}.content(u8"HEX / RGB / HSB")); });
            Button(ButtonProps{}.disabled(s.clear_disabled).onClick([this, id] {
                auto* s = services_->components().state<PickerState>(id);
                if (!s) {
                    return;
                }
                const auto callback = s->on_clear;
                publish(id, ColorPickerValue{}, true);
                if (services_->components().contains(id) && callback) {
                    callback();
                }
            }),
                   [locale = s.locale] { Text(TextProps{}.content(bind([locale] { return locale.get().clear; }))); });
            Button(ButtonProps{}.disabled(s.mode_disabled).onClick([this, id] {
                auto* s = services_->components().state<PickerState>(id);
                if (!s) {
                    return;
                }
                const auto value = s->value.get();
                if (value.gradient()) {
                    s->selected = value.stops().front().key;
                    publish(id, ColorPickerValue{value.stops().front().color}, true);
                    if (auto* live = services_->components().state<PickerState>(id)) {
                        live->cached_gradient = value;
                    }
                } else {
                    const auto cached = s->cached_gradient;
                    const auto color = selected_color(*s);
                    publish(id,
                            cached.value_or(ColorPickerValue{
                                std::vector<ColorStop>{{String{u8"start"}, color, 0}, {String{u8"end"}, color, 100}}}),
                            true);
                }
            }),
                   [locale = s.locale, value = s.value] {
                       Text(TextProps{}.content(bind([locale, value] {
                           const auto labels = locale.get();
                           return value.get().gradient() ? labels.gradient : labels.single;
                       })));
                   });
        });
        Flex(FlexProps{}.gap(SpaceSize::Small), [this, id] {
            auto& s = *services_->components().state<PickerState>(id);
            InputNumber(InputNumberProps{}
                            .value(s.stop_percent)
                            .min(0)
                            .max(100)
                            .disabled(s.stop_disabled)
                            .onChange([this, id](InputNumberValue v) {
                                if (v) {
                                    move_stop(id, *v, true);
                                }
                            })
                            .layout(LayoutStyle{}.width(dp(72))));
            Button(
                ButtonProps{}.disabled(s.stop_disabled).onClick([this, id] {
                    auto* s = services_->components().state<PickerState>(id);
                    if (!s) {
                        return;
                    }
                    const auto value = s->value.get();
                    if (value.stops().size() >= 64) {
                        return;
                    }
                    auto key = String::from_utf8("stop-" + std::to_string(s->next_stop++)).value();
                    while (std::ranges::find(value.stops(), key, &ColorStop::key) != value.stops().end()) {
                        key = String::from_utf8("stop-" + std::to_string(s->next_stop++)).value();
                    }
                    std::vector<ColorStop> stops{value.stops().begin(), value.stops().end()};
                    stops.push_back({key, value.sample(50), 50});
                    s->selected = key;
                    publish(id, ColorPickerValue{std::move(stops)}, true);
                }),
                [locale = s.locale] { Text(TextProps{}.content(bind([locale] { return locale.get().add_stop; }))); });
            Button(ButtonProps{}.disabled(s.remove_disabled).onClick([this, id] {
                auto* s = services_->components().state<PickerState>(id);
                if (!s) {
                    return;
                }
                const auto value = s->value.get();
                if (value.stops().size() <= 2) {
                    return;
                }
                std::vector<ColorStop> stops{value.stops().begin(), value.stops().end()};
                std::erase_if(stops, [&](auto& stop) { return stop.key == s->selected; });
                publish(id, ColorPickerValue{std::move(stops)}, true);
            }),
                   [locale = s.locale] { Text(TextProps{}.content(bind([locale] { return locale.get().remove; }))); });
        });
    });
    (void)build;
    (void)s;
}

void ColorPickerComponentHost::mount_presets(runtime::ComponentId id) {
    auto& b = runtime::require_component_build_context();
    auto* s = services_->components().state<PickerState>(id);
    if (!s) {
        return;
    }
    s->presets_root = b.mount_component<int>(0);
    const auto node = b.root(s->presets_root);
    services_->layout().set_layout(node, layout::FlexLayout{.direction = layout::FlexDirection::vertical,
                                                            .item_policy = layout::FlexItemPolicy::sequential});
    b.on_resource_cleanup(s->presets_root, [this, node] { services_->layout().remove_layout(node); });
    b.mount_slot(s->presets_root, Content{[this, id] {
                     const auto groups = services_->components().state<PickerState>(id)->presets;
                     for (const auto& group : groups) {
                         mount_preset_group(id, group);
                     }
                 }});
}

void ColorPickerComponentHost::mount_preset_group(runtime::ComponentId id, ColorPreset group) {
    auto& b = runtime::require_component_build_context();
    const auto row = b.mount_component<int>(0);
    const auto node = b.root(row);
    services_->layout().set_layout(node, layout::BoxLayout{});
    b.on_resource_cleanup(row, [this, node] { services_->layout().remove_layout(node); });
    services_->components().state<PickerState>(id)->preset_groups.push_back({group, row});
    b.mount_slot(
        row, Content{[this, id, group] {
            Flex(FlexProps{}.vertical(true).gap(SpaceSize::Small), [this, id, group] {
                auto expanded = std::make_shared<Signal<bool>>(group.default_open);
                Button(ButtonProps{}.onClick([expanded] { expanded->set(!expanded->get()); }),
                       [label = group.label] { Text(TextProps{}.content(label)); });
                // Preset buttons remain mounted, preserving retained content identity.
                auto& build = runtime::require_component_build_context();
                const auto container = build.mount_component<int>(0);
                const auto node = build.root(container);
                services_->layout().set_layout(node, layout::BoxLayout{});
                build.on_resource_cleanup(container, [this, node] { services_->layout().remove_layout(node); });
                connect_prop(build.scope(container), Prop<bool>{*expanded}, [this, container](bool shown) {
                    if (services_->components().set_branch_active(container, shown)) {
                        services_->mark_scene_structure_dirty();
                    }
                    services_->dirty().invalidate(services_->components().root(container),
                                                  runtime::DirtyFlags::Layout | runtime::DirtyFlags::Measure |
                                                      runtime::DirtyFlags::HitTest);
                });
                build.mount_slot(
                    container, Content{[this, id, group] {
                        Flex(FlexProps{}.wrap(true).gap(SpaceSize::Small), [this, id, group] {
                            for (std::size_t i = 0; i < group.colors.size(); ++i) {
                                Button(ButtonProps{}
                                           .disabled(services_->components().state<PickerState>(id)->disabled)
                                           .onClick([this, id, key = group.key, i] {
                                               auto* s = services_->components().state<PickerState>(id);
                                               if (!s) {
                                                   return;
                                               }
                                               const auto found = std::ranges::find(s->presets, key, &ColorPreset::key);
                                               if (found != s->presets.end() && i < found->colors.size()) {
                                                   publish(id, found->colors[i], true);
                                               }
                                           }),
                                       [color = group.colors[i]] { Text(TextProps{}.content(color.format())); });
                            }
                        });
                    }});
            });
        }});
}

void ColorPickerComponentHost::reconcile_presets(runtime::ComponentId id) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !s->presets_root.valid() || !s->presets_dirty) {
        return;
    }
    s->presets_dirty = false;
    const auto desired = s->presets;
    const auto old = s->preset_groups;
    const auto root = s->presets_root;
    for (const auto& [group, row] : old) {
        if (std::ranges::find(desired, group) == desired.end()) {
            services_->destroy(row);
            s = services_->components().state<PickerState>(id);
            if (!s) {
                return;
            }
            std::erase_if(s->preset_groups, [row](const auto& r) { return r.second == row; });
        }
    }
    for (const auto& group : desired) {
        s = services_->components().state<PickerState>(id);
        if (!s) {
            return;
        }
        if (std::ranges::find(s->preset_groups, group, [](const auto& r) { return r.first; }) ==
            s->preset_groups.end()) {
            services_->append_slot(root, Content{[this, id, group] { mount_preset_group(id, group); }});
        }
    }
    s = services_->components().state<PickerState>(id);
    if (!s) {
        return;
    }
    for (std::size_t i = 0; i < desired.size(); ++i) {
        const auto found = std::ranges::find(s->preset_groups, desired[i], [](const auto& r) { return r.first; });
        if (found != s->preset_groups.end()) {
            services_->nodes().require(services_->components().root(found->second)).external_layout.order =
                static_cast<int>(i);
        }
    }
    services_->mark_scene_structure_dirty();
    services_->dirty().invalidate(services_->components().root(root), runtime::DirtyFlags::Measure |
                                                                          runtime::DirtyFlags::Layout |
                                                                          runtime::DirtyFlags::HitTest);
}

void ColorPickerComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile_presets(id);
    }
}

void ColorPickerComponentHost::mount(const ColorPickerProps& p, ColorPickerSlots slots) {
    if ((p.value_ && p.initial_) || (p.format_ && p.initial_format_) || (p.open_ && p.initial_open_)) {
        throw std::invalid_argument("ColorPicker controlled/default props are mutually exclusive");
    }
    auto& h = *services_;
    auto& b = runtime::require_component_build_context();
    const auto id = b.mount_component<PickerState>();
    auto& s = b.state<PickerState>(id);
    const auto locale = localized_prop(p.locale_, b.config_scope(), [](const LocaleData& d) { return d.color_picker; });
    s.locale.set(read_prop(locale));
    s.node = b.root(id);
    s.controlled = p.value_.has_value();
    s.format_controlled = p.format_.has_value();
    s.open_controlled = p.open_.has_value();
    s.value.set(p.value_ ? read_prop(*p.value_) : p.initial_.value_or(ColorPickerValue{}));
    if (!s.controlled && read_prop(p.modes_) == std::vector<ColorPickerMode>{ColorPickerMode::Gradient} &&
        !s.value.get().gradient()) {
        const auto color = s.value.get().solid().value_or(PickerColor{});
        s.value.set(
            ColorPickerValue{std::vector<ColorStop>{{String{u8"start"}, color, 0}, {String{u8"end"}, color, 100}}});
    }
    s.format.set(p.format_ ? read_prop(*p.format_) : p.initial_format_.value_or(ColorFormat::Hex));
    s.open.set(p.open_ ? read_prop(*p.open_) : p.initial_open_.value_or(false));
    s.disabled.set(read_prop(p.disabled_));
    s.alpha_disabled.set(read_prop(p.disabled_alpha_));
    s.format_disabled.set(read_prop(p.disabled_format_));
    s.clear.set(read_prop(p.clear_));
    s.show_text.set(read_prop(p.show_text_));
    s.change = p.change_;
    s.complete = p.complete_;
    s.on_clear = p.on_clear_;
    s.format_change = p.format_change_;
    s.open_change = p.open_change_;
    s.modes = read_prop(p.modes_);
    s.presets = read_prop(p.presets_);
    h.layout().set_layout(s.node, layout::BoxLayout{});
    b.on_resource_cleanup(id, [this, id, node = s.node, alive = s.render_alive] {
        *alive = false;
        cancel(id);
        services_->interactions().set_input_blocked(id, false);
        services_->layout().remove_layout(node);
    });
    runtime::connect_layout_style(b.scope(id), p.layout_, s.node, h.nodes(), h.dirty());
    b.mount_slot(
        id, Content{[this, id, p, slots] {
            auto& s = *services_->components().state<PickerState>(id);
            auto props = PopoverProps{}
                             .open(s.open)
                             .disabled(s.disabled)
                             .trigger(p.trigger_)
                             .placement(p.placement_)
                             .arrow(p.arrow_)
                             .autoAdjustOverflow(p.adjust_)
                             .onOpenChange([this, id](bool open) {
                                 auto* s = services_->components().state<PickerState>(id);
                                 if (!s) {
                                     return;
                                 }
                                 const auto callback = s->open_change;
                                 if (!s->open_controlled) {
                                     s->open.set(open);
                                 }
                                 refresh(id);
                                 if (callback) {
                                     callback(open);
                                 }
                             });
            Popover(std::move(props), PopoverTrigger{[this, id, p, slots] {
                        auto& s = *services_->components().state<PickerState>(id);
                        if (slots.trigger) {
                            SlotContentAccess::function (*slots.trigger)();
                            return;
                        }
                        Button(ButtonProps{}.size(p.size_).disabled(s.disabled).color(bind([status = p.status_] {
                            const auto value = read_prop(status);
                            return value == InputStatus::Error     ? ButtonColor::Danger
                                   : value == InputStatus::Warning ? ButtonColor::Orange
                                                                   : ButtonColor::Default;
                        })),
                               [this, id, slots] {
                                   auto& s = *services_->components().state<PickerState>(id);
                                   if (slots.text_render) {
                                       slots.text_render({Prop<ColorPickerValue>{s.value},
                                                          Prop<String>{bind([value = s.value, format = s.format] {
                                                              return value.get().format(format.get());
                                                          })},
                                                          Prop<ColorFormat>{s.format}});
                                   } else {
                                       Text(TextProps{}.content(bind(
                                           [value = s.value, show = s.show_text, format = s.format, locale = s.locale,
                                            config = runtime::require_component_build_context().config_scope()] {
                                               return show.get()
                                                          ? display_label(
                                                                value.get(), format.get(),
                                                                config->snapshot().format_locale.pack.data().statistic)
                                                          : locale.get().color;
                                           })));
                                   }
                                   // Retained preview surface: the trigger displays alpha and gradients.
                                   auto& b = runtime::require_component_build_context();
                                   s.swatch = b.mount_component<int>(0);
                                   const auto node = b.root(s.swatch);
                                   services_->nodes().require(node).external_layout.width = 24;
                                   services_->nodes().require(node).external_layout.height = 20;
                                   services_->layout().set_layout(node, layout::BoxLayout{});
                                   b.on_resource_cleanup(s.swatch,
                                                         [this, node] { services_->layout().remove_layout(node); });
                                   const auto fragment = b.register_scene_fragment(
                                       s.swatch, runtime::SceneFragmentPlacement::before_children);
                                   const auto surface = services_->surfaces().create_content_range(fragment, {});
                                   s.preview = Field{s.swatch, node, {}, surface, {}, {}};
                                   b.on_resource_cleanup(s.swatch, [this, surface] {
                                       services_->surfaces().destroy_content_range(surface);
                                   });
                               });
                    }},
                    PopoverSlots{.content = PopoverContent{[this, id, slots] {
                                     const auto weak =
                                         std::weak_ptr{services_->components().state<PickerState>(id)->render_alive};
                                     ColorPickerPanelParts parts{
                                         ColorPickerPanel{[this, id, weak] {
                                             const auto alive = weak.lock();
                                             if (alive && *alive && services_->components().scope(id).active()) {
                                                 mount_picker(id);
                                             }
                                         }},
                                         ColorPickerPresets{[this, id, weak] {
                                             const auto alive = weak.lock();
                                             if (alive && *alive && services_->components().scope(id).active()) {
                                                 mount_presets(id);
                                             }
                                         }}};
                                     Flex(FlexProps{}.vertical(true).gap(SpaceSize::Small), [parts, slots] {
                                         if (slots.panel_render) {
                                             slots.panel_render(parts);
                                         } else {
                                             parts.renderPicker();
                                             parts.renderPresets();
                                         }
                                         if (slots.extra) {
                                             SlotContentAccess::function (*slots.extra)();
                                         }
                                     });
                                 }}});
        }});
    // Preview is kept outside the indexed interactive fields.

    mounted_.push_back(id);
    s.ready = true;
    connect_prop(b.scope(id), locale, [this, id](ColorPickerLocale value) {
        if (const auto* current = services_->components().state<PickerState>(id)) {
            const auto signal = current->locale;
            signal.set(std::move(value));
        }
    });
    auto connect = [&](const auto& prop, auto apply) {
        connect_prop(b.scope(id), prop, [this, id, apply](auto value) {
            if (auto* s = services_->components().state<PickerState>(id)) {
                apply(*s, std::move(value));
                refresh(id);
            }
        });
    };
    connect(p.direction_, [](auto& s, TextDirection v) {
        if (v != TextDirection::Auto && v != TextDirection::LeftToRight && v != TextDirection::RightToLeft) {
            throw std::invalid_argument("Invalid ColorPicker direction");
        }
        s.rtl = v == TextDirection::RightToLeft;
    });
    connect(p.disabled_, [](auto& s, bool v) { s.disabled.set(v); });
    connect(p.disabled_alpha_, [](auto& s, bool v) { s.alpha_disabled.set(v); });
    connect(p.disabled_format_, [](auto& s, bool v) { s.format_disabled.set(v); });
    connect(p.clear_, [](auto& s, bool v) { s.clear.set(v); });
    connect(p.show_text_, [](auto& s, bool v) { s.show_text.set(v); });
    if (p.value_) {
        connect(*p.value_, [this, id](auto& s, ColorPickerValue v) {
            const bool interrupted = s.gesture && v != s.candidate;
            s.value.set(std::move(v));
            if (interrupted) {
                cancel(id);
            }
        });
    }
    if (p.format_) {
        connect(*p.format_, [](auto& s, ColorFormat v) {
            if (v != ColorFormat::Hex && v != ColorFormat::Rgb && v != ColorFormat::Hsb) {
                throw std::invalid_argument("Invalid ColorFormat");
            }
            s.format.set(v);
        });
    }
    if (p.open_) {
        connect(*p.open_, [](auto& s, bool v) { s.open.set(v); });
    }
    connect(p.modes_, [](auto& s, std::vector<ColorPickerMode> modes) {
        if (modes.empty() || modes.size() > 2 || (modes.size() == 2 && modes[0] == modes[1])) {
            throw std::invalid_argument("ColorPicker modes must be nonempty and unique");
        }
        for (auto m : modes) {
            if (m != ColorPickerMode::Single && m != ColorPickerMode::Gradient) {
                throw std::invalid_argument("Invalid picker mode");
            }
        }
        s.modes = std::move(modes);
    });
    connect(p.presets_, [](auto& s, std::vector<ColorPreset> groups) {
        if (groups.size() > 64) {
            throw std::invalid_argument("ColorPicker supports at most 64 preset groups");
        }
        std::vector<String> keys;
        for (const auto& group : groups) {
            if (group.key.empty() || std::ranges::find(keys, group.key) != keys.end() || group.colors.size() > 512) {
                throw std::invalid_argument("Invalid ColorPicker preset keys or count");
            }
            keys.push_back(group.key);
        }
        if (s.presets != groups) {
            s.presets = std::move(groups);
            s.presets_dirty = true;
        }
    });
    const auto theme = b.theme_scope();
    s.theme[0] =
        theme->capture([this, id](auto) { refresh(id); }, [theme] { static_cast<void>(theme->color_picker_colors()); });
    s.theme[1] = theme->capture(
        [this, id](auto) {
            if (auto* s = services_->components().state<PickerState>(id)) {
                for (const auto& f : s->fields) {
                    services_->dirty().invalidate(f.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                              runtime::DirtyFlags::Geometry);
                }
            }
        },
        [theme] { static_cast<void>(theme->color_picker_metrics()); });
    refresh(id);
}

void ColorPickerComponentHost::pointer(runtime::ComponentId id, std::size_t index,
                                       input::PointerDispatchContext& event) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !available(*services_, id, *s) || !s->open.get()) {
        return;
    }
    if (event.kind() == input::PointerEventKind::cancel) {
        cancel(id);
        refresh(id);
        return;
    }
    const auto input = event.event();
    if (event.kind() == input::PointerEventKind::down && input.button == input::PointerButton::primary && !s->pointer) {
        if (!event.capture_pointer()) {
            return;
        }
        s->pointer = input.pointer;
        s->active_field = index;
        s->gesture = true;
        s->gesture_start = s->value.get();
        s->candidate = s->value.get();
        if (index == 3 && s->value.get().gradient()) {
            const auto& bounds = s->fields[index].bounds;
            const auto translation = services_->nodes().require(s->fields[index].node).translation;
            auto pos = std::clamp(double((input.x - bounds.x - translation.x) / bounds.width) * 100, 0.0, 100.0);
            if (s->rtl) {
                pos = 100 - pos;
            }
            const auto stops = s->value.get().stops();
            s->selected = std::min_element(stops.begin(), stops.end(), [&](auto& a, auto& b) {
                              return std::abs(a.percent - pos) < std::abs(b.percent - pos);
                          })->key;
        }
    }
    if (!s->pointer || *s->pointer != input.pointer) {
        return;
    }
    if (event.kind() == input::PointerEventKind::up && input.button == input::PointerButton::primary) {
        static_cast<void>(event.release_pointer_capture());
        complete(id);
        return;
    }
    if (event.kind() != input::PointerEventKind::move && event.kind() != input::PointerEventKind::down) {
        return;
    }
    const auto& node = services_->nodes().require(s->fields[index].node);
    const auto bounds = s->fields[index].bounds;
    auto u = std::clamp(double(input.x - bounds.x - node.translation.x) / std::max(1.0F, bounds.width), 0.0, 1.0);
    if (s->rtl) {
        u = 1 - u;
    }
    const auto v =
        std::clamp(double(input.y - bounds.y - node.translation.y) / std::max(1.0F, bounds.height), 0.0, 1.0);
    if (index == 3) {
        move_stop(id, u * 100, false);
        return;
    }
    auto hsb = selected_color(*s).hsb();
    if (index == 0) {
        hsb.s = u;
        hsb.b = 1 - v;
    } else if (index == 1) {
        hsb.h = u * 360;
    } else {
        if (s->alpha_disabled.get()) {
            return;
        }
        hsb.a = u;
    }
    publish(id, replace_color(*s, PickerColor{hsb}), false);
}

bool ColorPickerComponentHost::keyboard(runtime::ComponentId id, std::size_t index,
                                        const input::KeyboardInputEvent& event) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s || !available(*services_, id, *s) || !s->open.get()) {
        return false;
    }
    using input::Key;
    const auto key = event.key;
    if (key != Key::left && key != Key::right && key != Key::up && key != Key::down && key != Key::home &&
        key != Key::end) {
        return false;
    }
    if (event.action == input::KeyAction::up) {
        complete(id);
        return true;
    }
    if (!s->gesture) {
        s->gesture = true;
        s->gesture_start = s->value.get();
        s->candidate = s->value.get();
    }
    auto step = key == Key::left || key == Key::down ? -1.0 : 1.0;
    if (s->rtl && (key == Key::left || key == Key::right)) {
        step = -step;
    }
    if (index == 3) {
        const auto value = s->candidate;
        auto stop = std::ranges::find(value.stops(), s->selected, &ColorStop::key);
        if (stop != value.stops().end()) {
            move_stop(id, key == Key::home ? 0 : key == Key::end ? 100 : stop->percent + step, false);
        }
        return true;
    }
    auto h = selected_color(*s).hsb();
    if (index == 0) {
        if (key == Key::up || key == Key::down) {
            h.b = std::clamp(h.b + step * .01, 0.0, 1.0);
        } else {
            h.s = key == Key::home ? 0 : key == Key::end ? 1 : std::clamp(h.s + step * .01, 0.0, 1.0);
        }
    } else if (index == 1) {
        h.h = key == Key::home ? 0 : key == Key::end ? 360 : std::clamp(h.h + step, 0.0, 360.0);
    } else {
        if (s->alpha_disabled.get()) {
            cancel(id);
            return false;
        }
        h.a = key == Key::home ? 0 : key == Key::end ? 1 : std::clamp(h.a + step * .01, 0.0, 1.0);
    }
    publish(id, replace_color(*s, PickerColor{h}), false);
    return true;
}

void ColorPickerComponentHost::paint(runtime::ComponentId id, runtime::Size viewport, runtime::Rect document_clip) {
    auto* s = services_->components().state<PickerState>(id);
    if (!s) {
        return;
    }
    const auto& theme = services_->components().theme_scope(id)->snapshot();
    const auto t = theme.color_picker();
    const auto line_width = theme.seed().line_width;
    const auto checker_size = std::max(1.0F, t.rail_height * .5F);
    const auto value = s->gesture ? s->candidate : s->value.get();
    const auto selected = selected_color(*s);
    const auto hsb = selected.hsb();
    auto paint_fields = s->fields;
    if (s->preview) {
        paint_fields.push_back(*s->preview);
    }
    for (std::size_t i = 0; i < paint_fields.size(); ++i) {
        const auto& f = paint_fields[i];
        const auto& node = services_->nodes().require(f.node);
        const auto clip = graphics::EffectClip{
            1,
            services_->nodes().content_clip(f.node, services_->component_clip(f.component, viewport, document_clip))};
        const bool preview = f.component == s->swatch;
        const auto b = preview ? node.bounds : f.bounds;
        const auto sample = [&](float u, float v) {
            if (s->rtl) {
                u = 1 - u;
            }
            const auto background =
                (static_cast<int>(u * b.width / checker_size) + static_cast<int>(v * b.height / checker_size)) % 2
                    ? t.checker_dark
                    : t.checker_light;
            if (preview || i == 3) {
                return composite(value.sample(u * 100).color(), background);
            }
            if (i == 0) {
                return PickerColor{ColorHsb{hsb.h, u, 1 - v, 1}}.color();
            }
            if (i == 1) {
                return PickerColor{ColorHsb{u * 360.0, 1, 1, 1}}.color();
            }
            auto c = selected.rgb();
            c.a = u;
            return composite(PickerColor{c}.color(), background);
        };
        auto effects = graphics::sampled_color_field(b, node.translation, preview ? 32 : 64,
                                                     preview  ? 4
                                                     : i == 0 ? 32
                                                              : 4,
                                                     sample, clip);
        const bool visible = services_->components().branch_active(f.component) && (preview || s->open.get()) &&
                             (i != 2 || !s->alpha_disabled.get());
        for (auto& effect : effects) {
            effect.material.opacity = visible ? node.opacity : 0;
        }
        if (!preview) {
            auto marker = [&](float u, float v, Color color, float size) {
                if (s->rtl) {
                    u = 1 - u;
                }
                auto effect = graphics::make_shadow_effect(
                    {{b.x + u * b.width - size / 2, b.y + v * b.height - size / 2, size, size}, size / 2},
                    ShadowLayer{ShadowKind::outer, {}, 0, 0, color}, node.translation, clip);
                effect.material.opacity = visible ? node.opacity : 0;
                effects.push_back(effect);
            };
            if (i < 3) {
                const auto u = static_cast<float>(i == 0 ? hsb.s : i == 1 ? hsb.h / 360 : hsb.a);
                const auto v = static_cast<float>(i == 0 ? 1 - hsb.b : .5);
                marker(u, v, f.focus ? t.focus : t.thumb, t.thumb_size + 2 * line_width);
                marker(u, v, selected.color(), std::max(0.0F, t.thumb_size - 2 * line_width));
            } else {
                for (const auto& stop : value.stops()) {
                    marker(static_cast<float>(stop.percent / 100), .5F, stop.key == s->selected ? t.focus : t.thumb,
                           t.thumb_size + 2 * line_width);
                }
            }
        }
        if (!preview) {
            graphics::QuadInstance hit_quad;
            hit_quad.opacity = 0;
            hit_quad.bounds = {b.x, b.y, b.width, b.height};
            hit_quad.translation = {node.translation.x, node.translation.y};
            const std::array hit_quads{hit_quad};
            static_cast<void>(services_->surfaces().update(f.hit_surface, hit_quads));
        }
        static_cast<void>(services_->surfaces().update_content_effects(f.surface, effects));
    }
}

void ColorPickerComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (auto* s = services_->components().state<PickerState>(id)) {
            if (!available(*services_, id, *s)) {
                cancel(id);
            }
            paint(id, viewport, clip);
        }
    }
}

void ColorPickerComponentHost::on_window_active(bool active) {
    if (!active) {
        for (auto id : mounted_) {
            cancel(id);
        }
    }
}

void ColorPickerComponentHost::on_input_boundary_changed() {
    for (auto id : mounted_) {
        refresh(id);
    }
}
} // namespace ryn::detail

namespace ryn {
void ColorPickerPanelParts::renderPicker() const {
    detail::SlotContentAccess::function(picker)();
}

void ColorPickerPanelParts::renderPresets() const {
    detail::SlotContentAccess::function(presets)();
}

void ColorPicker(ColorPickerProps props, ColorPickerSlots slots) {
    if (!detail::active_picker) {
        throw std::logic_error("ColorPicker requires a Window component host");
    }
    detail::active_picker->mount(props, std::move(slots));
}
} // namespace ryn
