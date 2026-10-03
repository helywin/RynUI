#include "component/otp_component.hpp"
#include "component/input_component.hpp"
#include "component/otp_input_cell.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::detail {
struct OTPRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    std::optional<runtime::ComponentId> binding;
    std::function<bool()> focus;
    std::function<bool()> blur;

    void ensure_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("OTPRef requires its owner thread");
        }
    }
};

struct OTPTransaction final {
    std::optional<OTPCandidate> candidate;
    OTPCells partial;
    String full;
    std::function<void(const OTPCells&)> on_input;
    std::function<void(String)> on_change;
};

struct OTPCell final {
    Signal<String> value{String{}};
    InputRef reference;
    runtime::ComponentId component;
    std::optional<runtime::ComponentId> separator;
    std::shared_ptr<OTPTransaction> pending;
};

struct OTPState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    std::shared_ptr<OTPModel> model;
    std::vector<std::shared_ptr<OTPCell>> cells;
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<OTPMask> mask{OTPMask{}};
    Signal<InputPurpose> purpose{InputPurpose::Number};
    Signal<InputCapitalization> capitalization{InputCapitalization::None};
    Signal<bool> autocorrect{false};
    OTPDirection direction{OTPDirection::LeftToRight};
    std::optional<OTPSeparator> separator;
    std::function<void(const OTPCells&)> on_input;
    std::function<void(String)> on_change;
    std::function<void(std::size_t)> on_focus;
    std::function<void(std::size_t)> on_blur;
    std::shared_ptr<OTPRefState> reference;
    theme_runtime::Subscription theme_subscription;
    bool auto_focus{};
    bool resizing{};
};

namespace {
thread_local OTPComponentHost* active_otp_host{};

struct OTPSeparatorState {};

void validate_direction(OTPDirection direction) {
    if (direction != OTPDirection::LeftToRight && direction != OTPDirection::RightToLeft) {
        throw std::invalid_argument("Invalid OTP direction");
    }
}

template <class T> void validate_enum(T value, T last, const char* message) {
    if (value < T{} || value > last) {
        throw std::invalid_argument(message);
    }
}
} // namespace

void validate_otp_mask(const OTPMask& mask) {
    input::TextBoundaryMap boundaries;
    if (!boundaries.assign(mask.glyph.bytes()) || boundaries.grapheme_count() != 1 ||
        mask.glyph.bytes().find_first_of("\r\n") != std::string_view::npos) {
        throw std::invalid_argument("OTP mask must be one single-line grapheme");
    }
}

struct OTPPropsAccess final {
    static void mount(OTPComponentHost& host, OTPProps props, std::optional<OTPSeparator> separator) {
        if (!host.services_->input_runtime()) {
            throw std::logic_error("OTP requires a window Input host");
        }
        if (props.value_ && props.default_value_) {
            throw std::invalid_argument("OTP value and defaultValue are mutually exclusive");
        }
        if (props.reference_) {
            props.reference_->ensure_owner();
            if (props.reference_->binding) {
                throw std::invalid_argument("OTPRef is already bound");
            }
        }
        const auto initial = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(String{});
        auto model = std::make_shared<OTPModel>(read_prop(props.length_), initial.bytes(), props.formatter_);
        const auto length = read_prop(props.length_);
        model->set_length(length);
        validate_otp_mask(read_prop(props.mask_));
        validate_direction(read_prop(props.direction_));
        validate_enum(read_prop(props.size_), ControlSize::Large, "Invalid OTP size");
        validate_enum(read_prop(props.variant_), InputVariant::Underlined, "Invalid OTP variant");
        validate_enum(read_prop(props.status_), InputStatus::Error, "Invalid OTP status");
        validate_enum(read_prop(props.purpose_), InputPurpose::Number, "Invalid OTP purpose");
        validate_enum(read_prop(props.capitalization_), InputCapitalization::Letters, "Invalid OTP capitalization");
        auto& build = runtime::require_component_build_context();
        const auto id = build.mount_component<OTPState>();
        auto& state = build.state<OTPState>(id);
        state.component = id;
        state.node = build.root(id);
        state.model = std::move(model);
        state.separator = std::move(separator);
        state.on_input = std::move(props.on_input_);
        state.on_change = std::move(props.on_change_);
        state.on_focus = std::move(props.on_focus_);
        state.on_blur = std::move(props.on_blur_);
        state.reference = props.reference_;
        state.auto_focus = props.auto_focus_;
        build.on_resource_cleanup(id, [&host, id] {
            if (auto* state = host.find(id)) {
                state->model->retire();
                if (state->reference && state->reference->binding == id) {
                    state->reference->binding.reset();
                    state->reference->focus = {};
                    state->reference->blur = {};
                }
                static_cast<void>(host.services_->layout().remove_layout(state->node));
            }
            std::erase_if(host.mounted_, [id](const auto& item) { return item.component == id; });
        });
        if (state.reference) {
            state.reference->binding = id;
            state.reference->focus = [&host, id] {
                return host.focus(id, 0);
            };
            state.reference->blur = [&host, id] {
                return host.blur(id);
            };
        }
        auto& scope = build.scope(id);
        static_cast<void>(connect_prop(scope, props.size_, [&host, id](ControlSize value) {
            validate_enum(value, ControlSize::Large, "Invalid OTP size");
            if (auto* state = host.find(id)) {
                state->size.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.variant_, [&host, id](InputVariant value) {
            validate_enum(value, InputVariant::Underlined, "Invalid OTP variant");
            if (auto* state = host.find(id)) {
                state->variant.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.status_, [&host, id](InputStatus value) {
            validate_enum(value, InputStatus::Error, "Invalid OTP status");
            if (auto* state = host.find(id)) {
                state->status.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.disabled_, [&host, id](bool value) {
            if (auto* state = host.find(id)) {
                state->disabled.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.read_only_, [&host, id](bool value) {
            if (auto* state = host.find(id)) {
                state->read_only.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.mask_, [&host, id](const OTPMask& value) {
            validate_otp_mask(value);
            if (auto* state = host.find(id)) {
                state->mask.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.purpose_, [&host, id](InputPurpose value) {
            validate_enum(value, InputPurpose::Number, "Invalid OTP purpose");
            if (auto* state = host.find(id)) {
                state->purpose.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.capitalization_, [&host, id](InputCapitalization value) {
            validate_enum(value, InputCapitalization::Letters, "Invalid OTP capitalization");
            if (auto* state = host.find(id)) {
                state->capitalization.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.autocorrect_, [&host, id](bool value) {
            if (auto* state = host.find(id)) {
                state->autocorrect.set(value);
            }
        }));
        static_cast<void>(connect_prop(scope, props.direction_, [&host, id](OTPDirection value) {
            validate_direction(value);
            if (auto* state = host.find(id)) {
                state->direction = value;
                host.update_layout(id);
            }
        }));
        host.update_layout(id);
        runtime::connect_layout_style(scope, props.layout_, state.node, host.services_->nodes(),
                                      host.services_->dirty());
        host.mount_cells(id, 0, length);
        static_cast<void>(
            connect_prop(scope, props.length_, [&host, id](std::size_t value) { host.set_length(id, value); }));
        if (props.value_) {
            static_cast<void>(
                connect_prop(scope, *props.value_, [&host, id, first = true, initial](const String& value) mutable {
                    if (std::exchange(first, false) && value == initial) {
                        return;
                    }
                    if (auto* state = host.find(id); state && state->model->reconcile(value.bytes())) {
                        host.project(id);
                    }
                }));
        }
        const auto theme = host.services_->components().theme_scope(id);
        state.theme_subscription = theme->capture([&host, id](theme_runtime::DirtyPhase) { host.update_layout(id); },
                                                  [theme] { static_cast<void>(theme->map()); });
        host.mounted_.push_back({id, state.node});
    }
};

OTPComponentHost::OTPComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

OTPComponentHost::~OTPComponentHost() {
    services_->detach(*this);
}

void* OTPComponentHost::begin_mount() noexcept {
    return std::exchange(active_otp_host, this);
}

void OTPComponentHost::end_mount(void* previous) noexcept {
    active_otp_host = static_cast<OTPComponentHost*>(previous);
}

void OTPComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](const auto& item) { return !services_->components().contains(item.component); });
}

OTPState* OTPComponentHost::find(runtime::ComponentId id) const {
    return services_->components().state<OTPState>(id);
}

OTPCells OTPComponentHost::cells(runtime::ComponentId id) const {
    const auto* state = find(id);
    if (!state) {
        throw std::out_of_range("Retired OTP");
    }
    return state->model->projection();
}

std::vector<runtime::ComponentId> OTPComponentHost::cell_components(runtime::ComponentId id) const {
    const auto* state = find(id);
    if (!state) {
        throw std::out_of_range("Retired OTP");
    }
    std::vector<runtime::ComponentId> result;
    for (const auto& cell : state->cells) {
        result.push_back(cell->component);
    }
    return result;
}

void OTPComponentHost::update_layout(runtime::ComponentId id) {
    auto* state = find(id);
    if (!state) {
        return;
    }
    layout::FlexLayout layout;
    layout.main_gap = services_->components().theme_scope(id)->snapshot().map().size_xs;
    layout.align = layout::FlexAlign::center;
    layout.right_to_left = state->direction == OTPDirection::RightToLeft;
    services_->layout().set_layout(state->node, layout);
    services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void OTPComponentHost::mount_cells(runtime::ComponentId id, std::size_t begin, std::size_t end) {
    auto* state = find(id);
    const auto projected = state->model->projection();
    std::vector<std::shared_ptr<OTPCell>> added;
    std::vector<std::optional<OTPSeparatorContent>> separators;
    for (std::size_t i = begin; i < end; ++i) {
        auto cell = std::make_shared<OTPCell>();
        cell->value.set(i < projected.size() ? projected[i] : String{});
        added.push_back(std::move(cell));
        separators.push_back(i && state->separator ? (*state->separator)(i - 1) : std::nullopt);
        if (!find(id)) {
            throw std::runtime_error("OTP separator retired its group");
        }
    }
    state->cells.reserve(end);
    const Content mount{[&, this] {
        for (std::size_t i = begin; i < end; ++i) {
            auto& cell = *added[i - begin];
            if (const auto& separator = separators[i - begin]) {
                auto& build = runtime::require_component_build_context();
                const auto wrapper = build.mount_component<OTPSeparatorState>();
                const auto node = build.root(wrapper);
                services_->layout().set_layout(node, layout::BoxLayout{});
                build.on_resource_cleanup(wrapper,
                                          [this, node] { static_cast<void>(services_->layout().remove_layout(node)); });
                const auto before = services_->interactions().declaration_order().size();
                build.mount_slot(wrapper, *separator);
                if (services_->interactions().declaration_order().size() != before) {
                    throw std::invalid_argument("OTP separator must be passive");
                }
                cell.separator = wrapper;
            }
            InputProps props;
            props.value(cell.value)
                .size(state->size)
                .variant(state->variant)
                .status(state->status)
                .disabled(state->disabled)
                .readOnly(state->read_only)
                .purpose(state->purpose)
                .capitalization(state->capitalization)
                .autocorrect(state->autocorrect)
                .ref(cell.reference)
                .autoFocus(state->auto_focus && i == 0 && begin == 0)
                .onFocus([this, id, i] { focused(id, i); })
                .onBlur([this, id, i] {
                    if (auto* current = find(id); current && current->on_blur) {
                        auto callback = current->on_blur;
                        callback(i);
                    }
                });
            props.otp_ = std::make_shared<OTPInputCellConfig>();
            props.otp_->mask = state->mask;
            props.otp_->transform = [this, id, i](std::string_view text) {
                return prepare(id, i, text);
            };
            props.otp_->committed = [this, id, i] {
                committed(id, i);
            };
            props.otp_->aborted = [this, id, i] {
                if (const auto* current = find(id); current && i < current->cells.size()) {
                    current->cells[i]->pending.reset();
                }
            };
            props.otp_->clicked = [this, id, i] {
                static_cast<void>(focus(id, i));
            };
            props.otp_->keyboard = [this, id, i](const auto& event) {
                return keyboard(id, i, event);
            };
            Input(std::move(props));
            cell.component = services_->input_runtime()->mounted_inputs().back().component;
        }
    }};
    if (begin == 0) {
        runtime::require_component_build_context().mount_slot(id, mount);
    } else {
        services_->append_slot(id, mount);
    }
    state = find(id);
    if (!state) {
        throw std::runtime_error("OTP retired during mount");
    }
    state->cells.insert(state->cells.end(), added.begin(), added.end());
}

void OTPComponentHost::set_length(runtime::ComponentId id, std::size_t length) {
    OTPModel::validate_length(length);
    auto* state = find(id);
    if (!state || state->model->length() == length) {
        return;
    }
    if (state->resizing) {
        throw std::logic_error("Reentrant OTP length update");
    }
    const auto previous = state->cells.size();
    const auto focused_interaction = services_->focus().state().focused;
    bool transfer = false;
    for (std::size_t i = length; i < previous; ++i) {
        const auto mounted = services_->input_runtime()->mounted_inputs();
        const auto it = std::ranges::find(mounted, state->cells[i]->component, &MountedInputComponent::component);
        transfer |= it != mounted.end() && focused_interaction == it->interaction;
    }
    state->resizing = true;
    try {
        if (length > previous) {
            mount_cells(id, previous, length);
        }
    } catch (...) {
        if (auto* live = find(id)) {
            live->resizing = false;
        }
        throw;
    }
    state = find(id);
    if (!state) {
        return;
    }
    state->model->set_length(length);
    while (state->cells.size() > length) {
        const auto cell = state->cells.back();
        state->cells.pop_back();
        services_->destroy(cell->component);
        if (cell->separator) {
            services_->destroy(*cell->separator);
        }
        state = find(id);
        if (!state) {
            return;
        }
    }
    state->resizing = false;
    project(id);
    update_layout(id);
    if (transfer && !services_->focus().state().focused) {
        static_cast<void>(focus(id, length - 1));
    }
}

void OTPComponentHost::project(runtime::ComponentId id) {
    const auto* state = find(id);
    if (!state) {
        return;
    }
    const auto cells = state->cells;
    const auto projection = state->model->projection();
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (!find(id)) {
            return;
        }
        cells[i]->pending.reset();
        cells[i]->value.set(projection[i]);
    }
}

std::string OTPComponentHost::prepare(runtime::ComponentId id, std::size_t index, std::string_view text) {
    const auto* state = find(id);
    if (!state || index >= state->cells.size() || state->resizing) {
        throw std::runtime_error("Retired OTP edit");
    }
    const auto model = state->model;
    const auto cell = state->cells[index];
    cell->pending.reset();
    auto candidate = model->prepare(index, text);
    if (!candidate || !find(id)) {
        throw std::runtime_error("Conflicting OTP edit");
    }
    const auto output = std::string(candidate->cells[index].bytes());
    auto transaction = std::make_shared<OTPTransaction>();
    transaction->partial = candidate->cells;
    std::string full;
    for (const auto& part : candidate->cells) {
        full.append(part.bytes());
    }
    transaction->full = String::from_utf8(full).value();
    state = find(id);
    if (!state) {
        throw std::runtime_error("Retired OTP edit");
    }
    transaction->on_input = state->on_input;
    transaction->on_change = state->on_change;
    transaction->candidate = std::move(candidate);
    cell->pending = std::move(transaction);
    return output;
}

void OTPComponentHost::committed(runtime::ComponentId id, std::size_t index) {
    auto* state = find(id);
    if (!state || index >= state->cells.size() || !state->cells[index]->pending) {
        return;
    }
    const auto transaction = std::exchange(state->cells[index]->pending, {});
    auto& candidate = *transaction->candidate;
    const auto model = state->model;
    const auto next = candidate.next_index;
    const auto complete = candidate.complete_changed;
    if (!model->commit(candidate)) {
        return;
    }
    const auto revision = model->revision();
    project(id);
    if (transaction->on_input) {
        transaction->on_input(transaction->partial);
    }
    if (!find(id) || model->revision() != revision) {
        return;
    }
    if (complete && transaction->on_change) {
        transaction->on_change(transaction->full);
    }
    if (!find(id) || model->revision() != revision) {
        return;
    }
    static_cast<void>(focus(id, next));
}

bool OTPComponentHost::focus(runtime::ComponentId id, std::size_t index) {
    const auto* state = find(id);
    if (!state || state->resizing || state->cells.empty()) {
        return false;
    }
    index = std::min({index, state->cells.size() - 1, state->model->first_empty()});
    const auto reference = state->cells[index]->reference;
    return reference.focus({InputFocusCursor::All});
}

bool OTPComponentHost::blur(runtime::ComponentId id) {
    const auto* state = find(id);
    if (!state) {
        return false;
    }
    const auto cells = state->cells;
    for (const auto& cell : cells) {
        if (cell->reference.blur()) {
            return true;
        }
    }
    return false;
}

void OTPComponentHost::focused(runtime::ComponentId id, std::size_t index) {
    const auto* state = find(id);
    if (!state || state->resizing || index >= state->cells.size()) {
        return;
    }
    auto callback = state->on_focus;
    static_cast<void>(focus(id, index));
    if (find(id) && callback) {
        callback(index);
    }
}

bool OTPComponentHost::keyboard(runtime::ComponentId id, std::size_t index, const input::KeyboardInputEvent& event) {
    using input::Key;
    const auto* state = find(id);
    if (!state || state->resizing) {
        return true;
    }
    const auto other =
        event.primary_modifier == input::KeyModifier::control ? input::KeyModifier::meta : input::KeyModifier::control;
    const bool primary = input::has_modifier(event.modifiers, event.primary_modifier) &&
                         !input::has_modifier(event.modifiers, other) &&
                         !input::has_modifier(event.modifiers, input::KeyModifier::alt);
    const bool plain = !input::has_modifier(event.modifiers, input::KeyModifier::control) &&
                       !input::has_modifier(event.modifiers, input::KeyModifier::meta) &&
                       !input::has_modifier(event.modifiers, input::KeyModifier::alt);
    if (primary && (event.key == Key::z || event.key == Key::y)) {
        return true;
    }
    const bool back = plain && event.key == Key::backspace && state->model->cell_empty(index);
    if (!plain || (!back && event.key != Key::left && event.key != Key::right)) {
        return false;
    }
    if (event.action == input::KeyAction::down) {
        const bool previous = back || ((event.key == Key::left) != (state->direction == OTPDirection::RightToLeft));
        const auto target = previous ? (index ? index - 1 : 0) : std::min(index + 1, state->cells.size() - 1);
        static_cast<void>(focus(id, target));
    }
    return true;
}
} // namespace ryn::detail

namespace ryn {
OTPRef::OTPRef() : state_(std::make_shared<detail::OTPRefState>()) {}

bool OTPRef::bound() const {
    state_->ensure_owner();
    return state_->binding.has_value();
}

bool OTPRef::focus() const {
    state_->ensure_owner();
    auto callback = state_->focus;
    return callback && callback();
}

bool OTPRef::blur() const {
    state_->ensure_owner();
    auto callback = state_->blur;
    return callback && callback();
}

void OTP(OTPProps props, std::optional<OTPSeparator> separator) {
    if (!detail::active_otp_host) {
        throw std::logic_error("OTP requires an active window component host");
    }
    detail::OTPPropsAccess::mount(*detail::active_otp_host, std::move(props), std::move(separator));
}
} // namespace ryn
