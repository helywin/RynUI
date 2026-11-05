#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "gallery_page_controller.hpp"
#include "component/otp_component.hpp"
#include "component/float_button_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "component/table_component.hpp"
#include "component/tree_select_component.hpp"
#include "component/cascader_component.hpp"
#include "component/transfer_component.hpp"
#include "component/form_component.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/divider_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/menu_component.hpp"
#include "component/pagination_component.hpp"
#include "component/selection_component.hpp"
#include "component/steps_component.hpp"
#include "component/tabs_component.hpp"
#include "component/typography_component.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>

namespace {

using namespace ryn;
using namespace ryn::input;
using namespace rynui::example;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Fixture final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GalleryPageModel model;
    GallerySamplePage active;
    std::unique_ptr<GalleryPageController> pages;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    bool readable{};
    std::optional<std::span<const GallerySampleDescriptor>> registry;
    std::size_t footer_calls{};

    explicit Fixture(bool readable_labels = false) : readable(readable_labels) {
        surfaces.mount(Content{[this] {
                           Theme(ThemeProps{}.config(theme),
                                 ThemeContent{[] { Flex(FlexProps{}.vertical(true).align(FlexAlign::Start), [] {}); }});
                       }},
                       &inputs);
        const auto holder = services.components().root_components().front();
        pages = std::make_unique<GalleryPageController>(
            surfaces, model, frames, holder, [this](const GalleryPageRoute& route, const GalleryPageDraft& draft) {
                GallerySamplePageOptions options;
                options.readable_labels = readable;
                options.registry = registry;
                if (readable) {
                    options.footer = [this](const GallerySampleDescriptor&, Signal<LogicalLength> width) {
                        ++footer_calls;
                        Text(TextProps{}.content(u8"C++ 源码入口").layout(LayoutStyle{}.width(width)));
                    };
                }
                active = make_gallery_sample_page(route, draft, std::move(options));
                active.set_width(760);
                active.set_theme(theme.get());
                return GalleryPageContent{active.content, active.save, {}};
            });
    }

    void sync() {
        require(buttons.layout_and_synchronize({800, 900}, {0, 0, 800, 900}, {}, 0, true), "sample layout failed");
    }

    void show(std::string_view identity, GalleryInputFamily family = GalleryInputFamily::input) {
        if (!pages->request(model.find(identity)->route(family)) || !pages->process() || pages->failed()) {
            throw std::runtime_error("sample page navigation failed: " + std::string{identity} + " / " +
                                     pages->last_error());
        }
        sync();
    }

    runtime::ComponentId sample(std::string_view identity) {
        for (const auto& surface : surfaces.mounted_surfaces()) {
            if (surfaces.snapshot(surface.component).identity == identity) {
                return surface.component;
            }
        }
        throw std::runtime_error("required sample is not mounted");
    }

    bool within(runtime::ComponentId component, runtime::ComponentId owner) {
        for (auto current = std::optional{component}; current; current = services.components().parent(*current)) {
            if (*current == owner) {
                return true;
            }
        }
        return false;
    }

    ryn::detail::MountedInputComponent editor(std::string_view sample_id, std::string_view test_id) {
        const auto owner = sample(sample_id);
        const auto target = active.target(test_id);
        require(target.valid() && within(target, owner), "registered editor is outside its sample");
        for (const auto& input : inputs.mounted_inputs()) {
            if (within(input.component, target)) {
                return input;
            }
        }
        throw std::runtime_error("registered test target has no editor: " + std::string{test_id});
    }

    void replace(const ryn::detail::MountedInputComponent& input, String text) {
        require(services.focus().state().focused == input.interaction ||
                    services.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                "editor focus failed");
        services.focus().dispatch({input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        require(bool(inputs.dispatch(input::TextCommitted{std::move(text), inputs.sessions().active()})),
                "commit failed");
        sync();
    }

    void action(std::string_view sample_id, std::string_view test_id) {
        const auto owner = sample(sample_id);
        const auto target = active.target(test_id);
        require(target.valid() && within(target, owner), "registered command is outside its sample");
        for (const auto& button : buttons.mounted_buttons()) {
            if (within(button.component, target)) {
                require(services.focus().state().focused == button.interaction ||
                            services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
                        "sample command focus failed");
                key(input::Key::enter);
                return;
            }
        }
        throw std::runtime_error("registered command target has no button: " + std::string{test_id});
    }

    void key(input::Key key) {
        services.focus().dispatch({key, input::KeyAction::down});
        services.focus().dispatch({key, input::KeyAction::up});
        sync();
    }

    void activate(runtime::ComponentId component) {
        for (const auto id : services.interactions().declaration_order()) {
            const auto& record = services.interactions().require(id);
            if (within(record.component, component) && record.focusable && record.eligible &&
                services.interactions().branch_active(id)) {
                require(services.focus().state().focused == id ||
                            services.focus().request_focus(id, input::FocusModality::keyboard),
                        "activation focus failed");
                key(input::Key::enter);
                return;
            }
        }
        throw std::runtime_error("registered component has no eligible activation target");
    }

    void verify_factories() {
        std::size_t expected{};
        for (const auto& descriptor : gallery_samples()) {
            const auto& route = model.current();
            const bool selected = descriptor.kind == route.kind &&
                                  descriptor.component_identity == route.component_identity &&
                                  descriptor.family == route.family;
            require(active.sample_runs(descriptor.identity) == (selected ? 1 : 0), "unrelated page factory ran");
            expected += selected;
        }
        const auto mounted = surfaces.mounted_surfaces();
        const auto sample_count = std::ranges::count_if(mounted, [this](const auto& surface) {
            return surfaces.snapshot(surface.component).identity.starts_with("gallery.sample.");
        });
        require(static_cast<std::size_t>(sample_count) == expected, "inactive page surfaces remained alive");
    }

    void open(runtime::ComponentId tree) {
        const auto selector = services.tree_select().snapshot(tree).selector;
        const auto inputs = this->inputs.mounted_inputs();
        const auto found = std::ranges::find_if(inputs, [selector](const auto& e) { return e.component == selector; });
        require(found != inputs.end() &&
                    (services.focus().state().focused == found->interaction ||
                     services.focus().request_focus(found->interaction, input::FocusModality::keyboard)),
                "TreeSelect focus failed");
        key(input::Key::down);
        require(services.tree_select().snapshot(tree).open, "TreeSelect keyboard open failed");
    }
};

void test_compiled_source_metadata() {
    require(gallery_sample_sources().size() == gallery_samples().size(), "sample/source registries disagree");
    for (const auto& descriptor : gallery_samples()) {
        const auto* source = gallery_sample_source(descriptor.identity);
        require(source && source->source_function == descriptor.source_function &&
                    source->public_header == descriptor.public_header && source->source_line > 0 &&
                    source->cpp.find(std::string{"void "} + std::string{source->source_function} + "(") !=
                        std::string_view::npos &&
                    String::from_utf8(source->cpp).has_value(),
                "sample source/API is absent, invalid UTF-8 or detached from its compiled factory");
    }
    require(!gallery_sample_source("not-registered"), "unregistered example received fabricated C++");
}

void test_legacy_identity_ownership() {
    Fixture f;
    const auto targets = gallery_legacy_test_targets();
    require(targets.size() == 458 && gallery_legacy_test_ids().size() == targets.size(),
            "frozen legacy identity inventory was truncated");
    std::set<std::string_view> identities;
    for (const auto& target : targets) {
        require(identities.insert(target.identity).second, "legacy identity has multiple primary owners");
        const auto owner =
            std::ranges::find(gallery_samples(), target.sample_identity, &GallerySampleDescriptor::identity);
        require(owner != gallery_samples().end(), "legacy identity has no registered sample owner");
        const GalleryPageRoute route{owner->kind, std::string{owner->component_identity}, owner->family};
        require(f.model.valid(route), "legacy identity owner has an invalid route");
        if (!target.replacement.empty()) {
            f.pages->request(route);
            f.pages->process();
            require(!f.pages->failed() && f.model.current() == route, "replacement owner page mount failed");
            f.sync();
            require(f.active.target(target.replacement).valid() &&
                        f.within(f.active.target(target.replacement), f.sample(target.sample_identity)),
                    "legacy replacement is absent or belongs to another sample");
        }
        if (target.identity.starts_with("ant.")) {
            require(ryn::find_ant_design_token(target.identity), "legacy Token has no catalog entry");
        }
    }
}

void test_registry_and_factory_isolation() {
    Fixture f;
    std::set<std::string_view> identities;
    for (const auto& descriptor : gallery_samples()) {
        require(identities.insert(descriptor.identity).second && !descriptor.source_function.empty() &&
                    descriptor.public_header.starts_with("include/ryn/") &&
                    f.model.valid(GalleryPageRoute{descriptor.kind, std::string{descriptor.component_identity},
                                                   descriptor.family}),
                "sample identity/source/header/owner is invalid");
    }
    for (const auto& route :
         {f.model.find("ant.component.button")->route(), f.model.find("ant.component.input")->route(),
          f.model.find("ant.component.tree-select")->route(), f.model.find("ant.component.upload")->route()}) {
        f.pages->request(route);
        require(f.pages->process() && !f.pages->failed(), "registered page mount failed");
        f.sync();
        std::size_t expected{};
        for (const auto& descriptor : gallery_samples()) {
            const bool active = descriptor.kind == route.kind &&
                                descriptor.component_identity == route.component_identity &&
                                descriptor.family == route.family;
            require(f.active.sample_runs(descriptor.identity) == (active ? 1 : 0), "unrelated factory ran");
            expected += active;
        }
        require(f.surfaces.mounted_surfaces().size() == expected, "inactive sample surfaces remained mounted");
        require(f.active.telemetry().content_runs == 1, "page content was reconstructed during layout");
    }
    require(f.inputs.mounted_inputs().empty() && f.buttons.mounted_buttons().empty(), "Upload mounted a fake demo");
    require(
        std::ranges::any_of(f.services.text().mounted_texts(),
                            [&](const auto& text) {
                                return f.scene.text_state(text.scene).content().bytes().find("尚未提供稳定原生组件") !=
                                       std::string_view::npos;
                            }),
        "Upload omitted its honest explanation");
    f.show("ant.component.list");
    require(std::ranges::any_of(f.services.text().mounted_texts(),
                                [&](const auto& text) {
                                    return f.scene.text_state(text.scene).content().bytes().find("已弃用") !=
                                           std::string_view::npos;
                                }) &&
                f.buttons.mounted_buttons().empty(),
            "List lost its deprecated explanation or mounted a fake demo");
}

void test_complete_catalog_and_related_routes() {
    Fixture f;
    for (const auto& page : f.model.pages()) {
        f.show(page.entry->identity);
        f.verify_factories();
        require(f.active.telemetry().content_runs == 1, "catalog page reconstructed its content");
        if (!page.native_scope()) {
            require(f.surfaces.mounted_surfaces().empty(), "non-native entry mounted a fabricated example");
        }
    }
    for (const auto& related : gallery_related_samples()) {
        require(f.model.find(related.component_identity) && f.model.valid(related.route()),
                "related workflow has an invalid owner or route");
        require(related.target_kind == GalleryPageKind::tokens ||
                    std::ranges::any_of(gallery_samples(),
                                        [&related](const auto& sample) {
                                            return sample.identity == related.target_sample &&
                                                   sample.component_identity == related.target_component;
                                        }),
                "related workflow target does not exist");
    }
    require(f.pages->request({GalleryPageKind::tokens}) && f.pages->process() && !f.pages->failed(),
            "Token page navigation failed");
    f.sync();
    f.verify_factories();
    require(f.surfaces.mounted_surfaces().size() > 4 && f.active.telemetry().content_runs == 1,
            "Token palette/scales/shadows were not mounted");
    f.show("ant.component.upload");
    require(f.inputs.mounted_inputs().empty() && f.buttons.mounted_buttons().empty(),
            "Token controls survived page destruction");
}

void test_input_semantics_and_draft_round_trip() {
    Fixture f;
    f.show("ant.component.input");
    const auto controlled = f.editor("gallery.sample.input.basics", "gallery.input.controlled");
    const auto uncontrolled = f.editor("gallery.sample.input.basics", "gallery.input.uncontrolled");
    require(f.services.focus().request_focus(controlled.interaction, input::FocusModality::keyboard),
            "preedit focus failed");
    const auto session = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, session})) &&
                f.active.telemetry().input_changes == 0,
            "preedit incorrectly committed a draft");
    f.services.focus().clear_focus();
    f.replace(controlled, String{u8"你🙂"});
    f.replace(uncontrolled, String{u8"普通草稿"});
    require(f.active.telemetry().input_changes == 2 && f.active.telemetry().input_submits == 0,
            "editing changed original change/submit behavior");
    const auto topology = f.services.components().component_count();
    const auto scenes = f.scene.size();
    const auto effects = f.buttons.rounded_effects().live_count();
    const auto layers = f.inputs.text_layers(controlled.component);
    const auto runs = f.active.telemetry().content_runs;
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    f.theme.set(dark);
    f.active.set_width(480);
    f.sync();
    require(f.services.components().component_count() == topology && f.scene.size() == scenes &&
                f.buttons.rounded_effects().live_count() == effects && f.active.telemetry().content_runs == runs &&
                f.editor("gallery.sample.input.basics", "gallery.input.controlled").editor == controlled.editor &&
                f.inputs.text_layers(controlled.component).base == layers.base &&
                f.inputs.text_layers(controlled.component).selected == layers.selected &&
                f.inputs.text_layers(controlled.component).placeholder == layers.placeholder,
            "same-page theme/resize replaced retained editor or scene identities");
    require(f.inputs.synchronize_input_area(1, 800, 900) && f.platform.areas > 0,
            "sample host input area was not synchronized");
    f.key(input::Key::enter);
    require(f.active.telemetry().input_submits == 1, "Input submit callback was lost");
    for (const auto& [id, variant] : std::array{std::pair{"gallery.input.outlined", InputVariant::Outlined},
                                                std::pair{"gallery.input.filled", InputVariant::Filled},
                                                std::pair{"gallery.input.borderless", InputVariant::Borderless},
                                                std::pair{"gallery.input.underlined", InputVariant::Underlined}}) {
        const auto input = f.editor("gallery.sample.input.features", id);
        require(f.inputs.variant(input.component) == variant, "Input variant changed during extraction");
    }
    const auto count = f.editor("gallery.sample.input.features", "gallery.input.grapheme-count");
    require(f.inputs.count_value(count.component) == 4 && f.inputs.status(count.component) == InputStatus::Error,
            "grapheme count/exceed behavior changed");
    f.show("ant.component.button");
    f.show("ant.component.input");
    require(f.inputs.editors()
                        .require(f.editor("gallery.sample.input.basics", "gallery.input.controlled").editor)
                        .value() == "你🙂" &&
                f.inputs.editors()
                        .require(f.editor("gallery.sample.input.basics", "gallery.input.uncontrolled").editor)
                        .value() == "普通草稿",
            "controlled/uncontrolled committed text did not survive a round trip");
    require(!f.services.components().contains(controlled.component) &&
                !f.services.components().contains(uncontrolled.component),
            "draft cache retained old component identities");
    f.show("ant.component.input", GalleryInputFamily::text_area);
    const auto area = f.editor("gallery.sample.text-area.features", "gallery.text-area.outlined");
    require(f.inputs.editors().require(area.editor).mode() == input::TextEditMode::MultiLine,
            "TextArea lost multiline mode");
    f.replace(area, String{u8"第一行\n第二行"});
    f.show("ant.component.input", GalleryInputFamily::otp);
    require(f.services.otp().mounted().size() == 10, "OTP groups were lost");
    f.show("ant.component.input", GalleryInputFamily::text_area);
    require(f.inputs.editors()
                    .require(f.editor("gallery.sample.text-area.features", "gallery.text-area.outlined").editor)
                    .value() == "第一行\n第二行",
            "TextArea committed lines did not survive a family round trip");
}

void test_sensitive_and_tree_select_workflow() {
    Fixture f;
    f.show("ant.component.input", GalleryInputFamily::password);
    const auto password = f.editor("gallery.sample.password.features", "gallery.password.controlled-toggle");
    f.replace(password, String{u8"secret"});
    f.show("ant.component.tree-select");
    require(f.services.tree_select().mounted().size() == 6 && f.inputs.mounted_inputs().size() == 7,
            "TreeSelect selector/note/Form workflow was lost");
    runtime::ComponentId target;
    runtime::ComponentId field;
    const auto form = f.services.form().mounted().front();
    for (const auto id : f.services.tree_select().mounted()) {
        if (f.within(id, form)) {
            field = id;
        } else if (!target.valid()) {
            target = id;
        }
    }
    require(target.valid() && field.valid() && f.services.tree_select().snapshot(target).value == String{u8"guide"},
            "TreeSelect initial selection or Form owner changed");
    const auto note = f.editor("gallery.sample.tree-select.destination", "gallery.tree-select.note");
    f.replace(note, String{u8"保存交付备注"});
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.save");
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.value-authority");
    f.open(target);
    f.key(input::Key::home);
    f.key(input::Key::down);
    f.key(input::Key::enter);
    require(f.services.tree_select().snapshot(target).value == String{u8"guide"}, "rejected choice changed authority");
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.value-authority");
    f.open(target);
    f.key(input::Key::home);
    f.key(input::Key::down);
    f.key(input::Key::enter);
    require(f.services.tree_select().snapshot(target).value == String{u8"docs"}, "accepted parent choice was lost");
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.submit");
    const auto item = f.services.form().items().front();
    require(!f.services.form().snapshot(item).error.empty(), "required Form validation was lost");
    f.open(field);
    f.key(input::Key::home);
    f.key(input::Key::down);
    f.key(input::Key::down);
    f.key(input::Key::enter);
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.submit");
    require(f.services.form().snapshot(item).error.empty() &&
                f.services.tree_select().snapshot(field).value == String{u8"guide"},
            "Form selection/submit behavior changed");
    f.action("gallery.sample.tree-select.destination", "gallery.tree-select.close-authority");
    f.open(target);
    f.key(input::Key::escape);
    require(f.services.tree_select().snapshot(target).open, "controlled close refusal was lost");
    f.show("ant.component.button");
    require(f.services.tree_select().mounted().empty(), "refused popup survived page destruction");
    f.show("ant.component.tree-select");
    require(f.inputs.editors()
                    .require(f.editor("gallery.sample.tree-select.destination", "gallery.tree-select.note").editor)
                    .value() == "保存交付备注",
            "TreeSelect note draft was lost");
    const auto restored = f.services.tree_select().snapshot(f.active.target("gallery.tree-select.selector"));
    const auto restored_field = f.services.tree_select().snapshot(f.active.target("gallery.tree-select.form-selector"));
    require(restored.value == String{u8"docs"} && restored_field.value == String{u8"guide"} && !restored.open &&
                !restored_field.open,
            "TreeSelect value/Form draft or transient separation was lost");
    f.show("ant.component.input", GalleryInputFamily::password);
    require(f.inputs.editors()
                    .require(f.editor("gallery.sample.password.features", "gallery.password.controlled-toggle").editor)
                    .value() == "controlled",
            "password text survived a family exit");
}

void test_button_search_and_otp_contracts() {
    Fixture f;
    f.show("ant.component.button");
    const auto disabled = f.active.target("gallery.state.disabled");
    require(f.buttons.snapshot(disabled).disabled &&
                f.buttons.snapshot(f.active.target("gallery.state.loading")).loading,
            "Button disabled/loading state examples changed");
    for (const auto& [identity, variant] :
         std::array{std::pair{"outlined", ButtonVariant::Outlined}, std::pair{"dashed", ButtonVariant::Dashed},
                    std::pair{"solid", ButtonVariant::Solid}, std::pair{"filled", ButtonVariant::Filled},
                    std::pair{"text", ButtonVariant::Text}, std::pair{"link", ButtonVariant::Link}}) {
        require(f.buttons.snapshot(f.active.target(std::string{"gallery.button."} + identity)).variant == variant,
                "Button variant was lost during extraction");
    }
    require(f.buttons.snapshot(f.active.target("gallery.button.round-block")).block &&
                f.buttons.snapshot(f.active.target("gallery.button.ghost")).ghost,
            "Button block/ghost examples were lost");
    f.action("gallery.sample.button.variants", "gallery.button.loading-focus");
    require(f.active.telemetry().activations == 1 &&
                f.buttons.snapshot(f.active.target("gallery.button.icon-end")).loading_pending &&
                !f.buttons.snapshot(f.active.target("gallery.button.icon-end")).loading,
            "Button delayed loading/ref example changed");
    f.buttons.tick_animations(animation::AnimationTime::microseconds(160000));
    f.sync();
    require(f.buttons.snapshot(f.active.target("gallery.button.icon-end")).loading,
            "Button delayed loading did not start at its deadline");
    f.show("ant.component.input", GalleryInputFamily::search);
    require(f.inputs.mounted_inputs().size() == 7 && f.services.otp().mounted().empty() &&
                f.services.tree_select().mounted().empty(),
            "Search mounted unrelated Input families");
    const auto search = f.editor("gallery.sample.search.basics", "gallery.search.controlled");
    const auto large = f.editor("gallery.sample.search.basics", "gallery.search.large");
    f.replace(search, String{u8"搜索"});
    f.key(input::Key::enter);
    require(f.active.telemetry().search_submits == 1, "Search Enter did not submit");
    const auto search_owner = f.active.target("gallery.search.controlled");
    std::optional<ryn::detail::MountedButtonComponent> search_button;
    std::optional<ryn::detail::MountedButtonComponent> large_button;
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (f.within(button.component, search_owner)) {
            search_button = button;
        }
        if (f.within(button.component, f.active.target("gallery.search.large"))) {
            large_button = button;
        }
    }
    require(search_button && large_button, "Search did not reuse native Button controls");
    f.key(input::Key::tab);
    require(f.services.focus().state().focused == search_button->interaction, "Search Tab did not reach its button");
    f.key(input::Key::space);
    require(f.services.focus().request_focus(large_button->interaction, input::FocusModality::keyboard),
            "large Search focus failed");
    f.key(input::Key::enter);
    require(f.active.telemetry().search_submits == 3, "Search button submit callback was lost");
    require(
        f.inputs.variant(f.editor("gallery.sample.search.features", "gallery.search.filled-vector").component) ==
                InputVariant::Filled &&
            f.inputs.variant(f.editor("gallery.sample.search.features", "gallery.search.underlined-small").component) ==
                InputVariant::Underlined,
        "Search connected variants changed");
    f.replace(large, String{u8"非受控搜索"});
    f.show("ant.component.input", GalleryInputFamily::otp);
    const auto otp = f.active.target("gallery.otp.controlled");
    const auto cells = f.services.otp().cell_components(otp);
    require(cells.size() == 6, "controlled OTP lost its six cells");
    const auto mounted = f.inputs.mounted_inputs();
    const auto first =
        std::ranges::find_if(mounted, [&](const auto& input) { return input.component == cells.front(); });
    require(first != mounted.end(), "OTP cell has no window editor");
    f.replace(*first, String{u8"123456"});
    const auto values = f.services.otp().cells(otp);
    require(values == ryn::detail::OTPCells(
                          {String{u8"1"}, String{u8"2"}, String{u8"3"}, String{u8"4"}, String{u8"5"}, String{u8"6"}}),
            "OTP paste distribution/controlled value changed");
    for (const auto& input : f.inputs.mounted_inputs()) {
        require(f.inputs.editors().require(input.editor).mode() == input::TextEditMode::SingleLine,
                "OTP cell was not a retained single-line editor");
    }
    f.show("ant.component.input", GalleryInputFamily::search);
    require(f.inputs.editors()
                        .require(f.editor("gallery.sample.search.basics", "gallery.search.controlled").editor)
                        .value() == "搜索" &&
                f.inputs.editors()
                        .require(f.editor("gallery.sample.search.basics", "gallery.search.large").editor)
                        .value() == "非受控搜索",
            "Search controlled/uncontrolled drafts were lost");
    f.show("ant.component.input", GalleryInputFamily::otp);
    require(std::ranges::all_of(f.services.otp().cells(f.active.target("gallery.otp.controlled")),
                                [](const String& s) { return s.empty(); }),
            "OTP secret survived a family round trip");
}

void test_readable_labels_and_sample_footers() {
    Fixture f{true};
    for (const auto id : {"input", "select", "form", "tree-select", "time-picker", "descriptions", "avatar"}) {
        f.footer_calls = 0;
        f.show(std::string{"ant.component."} + id);
        f.verify_factories();
        std::size_t expected{};
        for (const auto& sample : gallery_samples()) {
            expected += sample.component_identity == f.model.current().component_identity &&
                        sample.family == f.model.current().family;
        }
        require(f.footer_calls == expected, "sample footer escaped current page ownership");
        for (const auto& mounted : f.services.text().mounted_texts()) {
            if (f.scene.text_state(mounted.scene).content().bytes().starts_with("gallery.")) {
                throw std::runtime_error("production caption on " + std::string{id} + ": " +
                                         std::string{f.scene.text_state(mounted.scene).content().bytes()});
            }
        }
    }
    f.show("ant.component.input");
    f.replace(f.editor("gallery.sample.input.basics", "gallery.input.uncontrolled"), String{u8"gallery.user · 用户值"});
    f.show("ant.component.upload");
    f.show("ant.component.input");
    require(f.inputs.editors()
                    .require(f.editor("gallery.sample.input.basics", "gallery.input.uncontrolled").editor)
                    .value() == "gallery.user · 用户值",
            "readable captions altered a committed user draft");
}

void test_remaining_workflows_and_drafts() {
    Fixture f;
    f.show("ant.component.select");
    auto select = f.active.target("gallery.select.outlined");
    auto selector = f.services.select().snapshot(select).selector;
    require(f.services.select().snapshot(select).value == SelectSelection{u8"desktop"}, "Select default changed");
    // Clear the real native clear action; a null draft must stay uncontrolled and empty.
    bool cleared{};
    for (const auto id : f.services.interactions().declaration_order()) {
        const auto record = f.services.interactions().require(id);
        if (!record.focusable && record.eligible && record.component != selector &&
            f.within(record.component, selector)) {
            const auto b = f.nodes.require(f.services.components().root(record.component)).bounds;
            const auto x = b.x + b.width / 2;
            const auto y = b.y + b.height / 2;
            f.services.pointer().dispatch(
                {input::PointerIdentity::mouse(), input::PointerAction::down, input::PointerButton::primary, x, y});
            f.services.pointer().dispatch(
                {input::PointerIdentity::mouse(), input::PointerAction::up, input::PointerButton::primary, x, y});
            f.sync();
            cleared = true;
            break;
        }
    }
    require(cleared && !f.services.select().snapshot(select).value, "Select clear did not execute");
    f.show("ant.component.modal");
    f.action("gallery.sample.modal.dialogs", "gallery.modal.retained-edit");
    const auto dialog = f.active.target("gallery.modal.retained-edit.dialog");
    require(f.services.modal().snapshot(dialog).visible, "Modal trigger lost its controlled open state");
    f.replace(f.editor("gallery.sample.modal.dialogs", "gallery.modal.account"), String{u8"账户草稿"});
    f.show("ant.component.table");
    require(f.services.modal().mounted().empty() && !f.services.modal().visible(), "old Modal boundary survived");
    auto table = f.active.target("gallery.table.editable");
    const auto before = f.services.table().snapshot(table);
    require(before.visible_keys.size() == 4 && f.active.target("gallery.table.menu.inline").valid() &&
                f.active.target("gallery.table.pagination").valid(),
            "Table linked workflow was split or duplicated");
    f.action("gallery.sample.table.workflow", "gallery.table.update");
    const auto row_key = before.visible_keys.front();
    const auto row_name = [&] {
        const auto snapshot = f.services.table().snapshot(table);
        const auto row = std::ranges::find(snapshot.rows, row_key, &ryn::detail::TableRowSnapshot::key);
        require(row != snapshot.rows.end(), "Table business row disappeared");
        for (const auto& text : f.services.text().mounted_texts()) {
            if (f.within(text.component, row->cells.at(1))) {
                return std::string{f.scene.text_state(text.scene).content().bytes()};
            }
        }
        throw std::runtime_error("Table row lost its task name");
    };
    require(row_name() == "已更新任务", "Table update no longer propagates to its retained cell");
    f.show("ant.component.select");
    select = f.active.target("gallery.select.outlined");
    require(!f.services.select().snapshot(select).value, "cleared Select default reappeared after navigation");
    f.show("ant.component.modal");
    require(!f.services.modal().visible(), "transient Modal open state was restored as a draft");
    f.action("gallery.sample.modal.dialogs", "gallery.modal.retained-edit");
    require(
        f.inputs.editors().require(f.editor("gallery.sample.modal.dialogs", "gallery.modal.account").editor).value() ==
            "账户草稿",
        "Modal account draft was lost");
    f.show("ant.component.table");
    table = f.active.target("gallery.table.editable");
    require(row_name() == "已更新任务", "Table business records did not survive a round trip");
}

void test_layout_and_navigation_pages() {
    Fixture f;
    for (const auto identity : {"typography", "divider", "flex", "space", "icon", "breadcrumb", "steps", "menu",
                                "dropdown", "pagination", "tabs"}) {
        f.show(std::string{"ant.component."} + identity);
        f.verify_factories();
        require(f.active.telemetry().content_runs == 1 && f.services.tree_select().mounted().empty(),
                "layout/navigation page constructed an unrelated workflow");
    }
    f.show("ant.component.typography");
    const auto ellipsis = f.active.target("gallery.typography.ellipsis");
    const auto text = f.services.typography().snapshot(ellipsis);
    require(text.truncated && text.copy_available && !text.expanded &&
                text.original.bytes().size() > text.displayed.bytes().size(),
            "Typography ellipsis/copy original was lost");
    const auto rich = f.services.typography().snapshot(f.active.target("gallery.typography.rich-content"));
    require(rich.copy_available && rich.original.bytes().find("code <>&\"") != std::string_view::npos &&
                rich.original.bytes().find('\n') != std::string_view::npos,
            "Typography rich content sample did not mount the production component");
    f.activate(f.active.target("gallery.typography.link"));
    require(f.active.telemetry().activations == 1, "Typography Link callback was lost");
    f.show("ant.component.divider");
    const auto dotted = f.services.divider().snapshot(f.active.target("gallery.divider.small-dotted-rtl"));
    require(dotted.variant == DividerVariant::Dotted && dotted.direction == DividerDirection::RightToLeft &&
                f.services.divider().snapshot(f.active.target("gallery.divider.vertical-dotted")).vertical,
            "Divider variant/direction examples changed");
    f.show("ant.component.flex");
    const auto stretch = f.services.components().root(f.active.target("gallery.flex.default-stretch"));
    const auto children = f.nodes.require(stretch).children;
    require(children.size() == 2 && f.nodes.require(children[0]).bounds.height == 64 &&
                f.nodes.require(children[1]).bounds.height == 22,
            "Flex default stretch behavior changed");
    f.show("ant.component.space");
    for (const auto identity : {"separator", "baseline", "compact.small", "compact.middle", "compact.large",
                                "compact.vertical-rtl", "compact.mixed", "addon.variants"}) {
        require(f.active.target(std::string{"gallery.space."} + identity).valid(), "Space composition target was lost");
    }
    f.show("ant.component.icon");
    const auto source = f.active.target("gallery.icon.retained-source");
    const auto before = f.services.text().icon_snapshot(source);
    f.action("gallery.sample.icon.catalog", "gallery.icon.rotate");
    require(f.services.text().icon_snapshot(source).angle_degrees == 45, "retained Icon rotation changed");
    f.action("gallery.sample.icon.catalog", "gallery.icon.change-source");
    require(f.services.text().icon_snapshot(source).source != before.source, "reactive Icon source did not change");
    f.action("gallery.sample.icon.catalog", "gallery.icon.toggle-spin");
    require(f.services.text().icon_snapshot(f.active.target("gallery.icon.spin")).spin, "Icon spin control changed");
    f.show("ant.component.breadcrumb");
    const auto trail = f.active.target("gallery.breadcrumb.component");
    const auto initial = f.services.breadcrumb().snapshot(trail);
    require(initial.items.size() == 3, "Breadcrumb typed items changed");
    const auto draft = f.editor("gallery.sample.breadcrumb.trail", "gallery.breadcrumb.draft");
    f.replace(draft, String{u8"层级草稿"});
    f.action("gallery.sample.breadcrumb.trail", "gallery.breadcrumb.visible");
    require(!f.services.components().branch_active(initial.items[1]), "Breadcrumb visibility did not update");
    f.action("gallery.sample.breadcrumb.trail", "gallery.breadcrumb.restore");
    require(f.services.components().branch_active(initial.items[1]) &&
                f.inputs.editors().require(draft.editor).value() == "层级草稿",
            "Breadcrumb restore lost retained editor");
    f.show("ant.component.steps");
    const auto steps = f.active.target("gallery.steps.component");
    const auto note = f.editor("gallery.sample.steps.workflow", "gallery.steps.draft");
    f.replace(note, String{u8"流程备注"});
    f.action("gallery.sample.steps.workflow", "gallery.steps.next");
    require(f.services.steps().snapshot(steps).current == 1, "Steps Next did not advance controlled state");
    f.action("gallery.sample.steps.workflow", "gallery.steps.status");
    require(f.services.steps().snapshot(steps).statuses[1] == StepsStatus::Error, "Steps controlled status changed");
    f.action("gallery.sample.steps.workflow", "gallery.steps.reset");
    require(f.services.steps().snapshot(steps).current == 0 &&
                f.inputs.editors().require(note.editor).value() == "流程备注",
            "Steps reset lost its retained note");
    f.show("ant.component.menu");
    const auto menu = f.active.target("gallery.menu.inline");
    f.services.menu().focus_menu(menu);
    f.key(input::Key::home);
    f.key(input::Key::down);
    f.key(input::Key::enter);
    require(f.services.menu().snapshot(menu).selected_keys == MenuKeys{String{u8"tasks"}},
            "Menu selection callback changed");
    f.action("gallery.sample.menu.inline", "gallery.menu.disabled");
    require(!f.services.interactions().require(f.services.menu().snapshot(menu).interaction).eligible,
            "Menu disabled control changed");
    const auto dynamic_menu = f.active.target("gallery.menu.dynamic");
    require(dynamic_menu.valid() && f.services.menu().snapshot(dynamic_menu).overflow,
            "Gallery horizontal Menu overflow sample missing");
    f.services.menu().focus_menu(dynamic_menu);
    f.key(input::Key::end);
    f.key(input::Key::down);
    require(!f.services.menu().snapshot(dynamic_menu).popups.empty(), "Gallery retained overflow popup did not open");
    f.show("ant.component.dropdown");
    require(!f.services.components().contains(dynamic_menu), "Gallery page switch retained Menu flyouts");
    const auto dropdown = f.active.target("gallery.dropdown.actions");
    f.action("gallery.sample.dropdown.actions", "gallery.dropdown.actions");
    require(f.services.dropdown().snapshot(dropdown).geometry.visible, "Dropdown command trigger did not open");
    f.key(input::Key::escape);
    require(!f.services.dropdown().snapshot(dropdown).geometry.visible, "Dropdown Escape did not close");
    const auto custom_dropdown = f.active.target("gallery.dropdown.custom");
    require(custom_dropdown.valid() && f.active.target("gallery.dropdown.split").valid(),
            "new Dropdown samples missing");
    const auto custom_before = f.services.dropdown().snapshot(custom_dropdown);
    f.action("gallery.sample.dropdown.actions", "gallery.dropdown.custom");
    require(f.services.dropdown().snapshot(custom_dropdown).geometry.visible &&
                f.services.dropdown().snapshot(custom_dropdown).arrow_visible,
            "Gallery retained custom popup did not open");
    f.key(input::Key::escape);
    f.action("gallery.sample.dropdown.actions", "gallery.dropdown.custom");
    require(f.services.dropdown().snapshot(custom_dropdown).content == custom_before.content,
            "Gallery popup render lost retained content");
    f.show("ant.component.pagination");
    const auto pagination = f.active.target("gallery.pagination.controlled");
    const auto pages = f.services.pagination().snapshot(pagination);
    require(pages.pages == 3 && pages.page_size == 4 && pages.current == 1, "Pagination total/page size changed");
    const auto next = std::ranges::find(pages.targets, std::size_t{2});
    require(next != pages.targets.end(), "Pagination target page is missing");
    f.activate(pages.items[static_cast<std::size_t>(next - pages.targets.begin())]);
    require(f.services.pagination().snapshot(pagination).current == 2, "Pagination controlled callback changed");
    f.show("ant.component.tabs");
    const auto tabs = f.active.target("gallery.tabs.controlled-retained");
    const auto panels = f.services.tabs().snapshot(tabs);
    require(panels.key == String{u8"draft"} && panels.panels.size() == 3, "Tabs typed panels changed");
    std::optional<ryn::detail::MountedInputComponent> retained;
    for (const auto& input : f.inputs.mounted_inputs()) {
        if (f.within(input.component, panels.panels[0])) {
            retained = input;
            break;
        }
    }
    require(retained.has_value(), "Tabs draft editor was lost");
    f.replace(*retained, String{u8"标签草稿"});
    f.activate(panels.headers[1]);
    require(f.services.tabs().snapshot(tabs).key == String{u8"settings"}, "Tabs header did not select its panel");
    f.activate(panels.headers[0]);
    require(f.inputs.editors().require(retained->editor).value() == "标签草稿" &&
                f.services.tabs().snapshot(tabs).panels == panels.panels,
            "Tabs round trip replaced retained state");
}

void test_float_button_page() {
    Fixture f;
    f.show("ant.component.float-button");
    require(f.active.sample_runs("gallery.sample.float-button.actions") == 1 &&
                !f.services.float_button().mounted().empty(),
            "FloatButton canonical page has no real sample");
    runtime::ComponentId floating;
    runtime::ComponentId back_top;
    for (auto id : f.services.float_button().mounted()) {
        if (f.services.components().in_window_layer(id)) {
            floating = id;
        }
        if (f.services.float_button().snapshot(id).progress_view.valid()) {
            back_top = id;
        }
    }
    require(floating.valid() && back_top.valid(), "FloatButton page missed window layer or BackTop");
    const auto r = f.nodes.require(f.services.components().root(floating)).bounds;
    require(r.x == 736 && r.y == 812, "FloatButton page window action did not anchor to viewport");
    std::optional<InteractionId> back_action;
    for (const auto& button : f.buttons.mounted_buttons()) {
        if (f.within(button.component, back_top)) {
            back_action = button.interaction;
            break;
        }
    }
    require(back_action && f.services.focus().request_focus(*back_action, FocusModality::keyboard),
            "Gallery BackTop is not keyboard reachable");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    f.sync();
    require(!f.services.float_button().snapshot(back_top).visible,
            "Gallery BackTop did not update its explicit scroll source");
    f.show("ant.component.button");
    require(f.services.float_button().mounted().empty(), "Off-page FloatButton resources survived page transition");
}

void test_cascader_transfer_pages() {
    Fixture f;
    const auto activate_button = [&](runtime::ComponentId component) {
        const auto it =
            std::ranges::find_if(f.buttons.mounted_buttons(), [&](const auto& b) { return b.component == component; });
        require(it != f.buttons.mounted_buttons().end(), "native Gallery button missing");
        require(f.services.focus().request_focus(it->interaction, FocusModality::keyboard),
                "native Gallery button cannot focus");
        f.key(Key::enter);
    };
    f.show("ant.component.cascader");
    require(f.services.cascader().mounted().size() == 7, "Cascader standalone examples missing");
    auto id = f.services.cascader().mounted()[1];
    auto cascader = f.services.cascader().snapshot(id);
    activate_button(cascader.rows[0].component);
    require(f.services.cascader().snapshot(id).value.size() == 2,
            "Cascader Gallery multi callback did not select leaves");
    const auto tag_id = f.services.cascader().mounted()[3];
    auto tags = f.services.cascader().snapshot(tag_id);
    require(tags.tags.size() == 3 && tags.tags[0].remove.valid(), "Cascader Gallery retained tags absent");
    const auto shown = std::ranges::find_if(tags.tags, [](const auto& tag) { return tag.shown; });
    require(shown != tags.tags.end(), "Cascader Gallery responsive fitting hid every tag");
    activate_button(shown->remove);
    require(f.services.cascader().snapshot(tag_id).tags.size() == 2, "Cascader Gallery tag removal callback failed");
    f.show("ant.component.transfer");
    require(f.services.cascader().mounted().empty() && f.services.transfer().mounted().size() == 3,
            "Cascader leave/Transfer mount failed");
    id = f.services.transfer().mounted()[0];
    auto transfer = f.services.transfer().snapshot(id);
    const auto selected = std::ranges::find_if(f.selections.mounted(),
                                               [&](const auto& c) { return c.component == transfer.rows[0].checkbox; });
    require(selected != f.selections.mounted().end() &&
                f.services.focus().request_focus(selected->interaction, FocusModality::keyboard),
            "Transfer Gallery Checkbox missing");
    f.key(Key::space);
    activate_button(transfer.move[1]);
    require(f.services.transfer().snapshot(id).target_keys.size() == 3,
            "Transfer Gallery controlled callback did not accept move");
    const auto custom = f.services.transfer().mounted()[2];
    const auto owns = [&](runtime::ComponentId child, runtime::ComponentId parent) {
        for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
            if (*current == parent) {
                return true;
            }
        }
        return false;
    };
    const auto table = std::ranges::find_if(f.services.table().mounted(), [&](auto value) {
        return owns(value, custom) && !f.services.table().snapshot(value).rows.empty();
    });
    require(table != f.services.table().mounted().end(), "Transfer Gallery custom native Table missing");
    const auto row = f.services.table().snapshot(*table).rows.front().checkbox;
    const auto cell = std::ranges::find_if(f.selections.mounted(), [&](const auto& c) { return c.component == row; });
    require(cell != f.selections.mounted().end() &&
                f.services.focus().request_focus(cell->interaction, FocusModality::keyboard),
            "Transfer custom Table row selection missing");
    f.key(Key::space);
    require(f.services.transfer().snapshot(custom).selected_keys.size() == 1,
            "Transfer custom Table selection callback did not update context");
    const auto action = f.services.transfer().snapshot(custom).move[1];
    const auto button = std::ranges::find_if(f.buttons.mounted_buttons(),
                                             [&](const auto& value) { return owns(value.component, action); });
    require(button != f.buttons.mounted_buttons().end(), "Transfer custom action missing");
    activate_button(button->component);
    require(f.services.transfer().snapshot(custom).target_keys.size() == 1,
            "Transfer custom native Table/action move did not apply");
    f.show("ant.component.button");
    require(f.services.transfer().mounted().empty(), "Transfer survived page leave");
}

} // namespace

void test_complete_form_examples() {
    const auto select = [](Fixture& f, std::string_view identity) {
        const auto all = gallery_samples();
        const auto it = std::ranges::find(all, identity, &GallerySampleDescriptor::identity);
        require(it != all.end(), "Form sample descriptor missing");
        f.registry = std::span<const GallerySampleDescriptor>{&*it, 1};
        f.show("ant.component.form");
    };
    const auto has_text = [](Fixture& f, std::string_view expected) {
        for (const auto& text : f.services.text().mounted_texts()) {
            if (f.scene.text_state(text.scene).content().bytes().find(expected) != std::string_view::npos) {
                return true;
            }
        }
        return false;
    };
    {
        Fixture f;
        select(f, "gallery.sample.form.layouts");
        f.action("gallery.sample.form.layouts", "gallery.form.layouts.reveal");
        const auto name = f.editor("gallery.sample.form.layouts", "gallery.form.layouts.name");
        require(f.services.focus().state().focused == name.interaction,
                "Form Gallery native reveal did not focus actual input");
        f.replace(name, String{u8"Native"});
        f.action("gallery.sample.form.layouts", "gallery.form.layouts.submit");
        require(has_text(f, "name=Native amount=9007199254740993.0001"),
                "Form Gallery submitted fabricated or rounded values");
        const auto runs = f.active.sample_runs("gallery.sample.form.layouts");
        f.action("gallery.sample.form.layouts", "gallery.form.layouts.toggle");
        require(f.active.sample_runs("gallery.sample.form.layouts") == runs, "Form layout update rebuilt the sample");
    }
    {
        Fixture f;
        select(f, "gallery.sample.form.lists");
        const auto first = f.inputs.mounted_inputs().front();
        f.replace(first, String{u8"Member"});
        const auto count = f.inputs.mounted_inputs().size();
        f.action("gallery.sample.form.lists", "gallery.form.list.add");
        require(f.inputs.mounted_inputs().size() == count + 2 && f.services.components().contains(first.component),
                "Form Gallery add remounted rich rows");
        f.action("gallery.sample.form.lists", "gallery.form.list.submit");
        require(has_text(f, "people/0/name/Member"), "Form Gallery list result omitted actual deep values");
        f.action("gallery.sample.form.lists", "gallery.form.list.reset");
        require(f.inputs.mounted_inputs().size() == count && f.inputs.editors().require(first.editor).value().empty(),
                "Form Gallery list reset failed");
    }
    {
        Fixture f;
        select(f, "gallery.sample.form.async");
        const auto name = f.editor("gallery.sample.form.async", "gallery.form.async.name");
        const auto confirmation = f.editor("gallery.sample.form.async", "gallery.form.async.confirmation");
        f.replace(name, String{u8"Ada"});
        f.replace(confirmation, String{u8"Ada"});
        f.action("gallery.sample.form.async", "gallery.form.async.submit");
        require(has_text(f, "正在校验当前快照"), "Form Gallery async submit did not remain pending");
        f.action("gallery.sample.form.async", "gallery.form.async.deliver");
        require(has_text(f, "name=Ada confirmation=Ada"),
                "Form Gallery async result did not use actual submitted values");
        f.action("gallery.sample.form.async", "gallery.form.async.submit");
        f.replace(name, String{u8"Grace"});
        f.action("gallery.sample.form.async", "gallery.form.async.deliver");
        require(has_text(f, "旧快照已取消"), "Form Gallery published stale async success");
        f.show("ant.component.button");
        require(f.services.form().mounted().empty(), "leaving Form Gallery retained Form scope");
    }
}

int main() {
    try {
        test_compiled_source_metadata();
        test_cascader_transfer_pages();
        test_legacy_identity_ownership();
        test_float_button_page();
        test_registry_and_factory_isolation();
        test_complete_catalog_and_related_routes();
        test_input_semantics_and_draft_round_trip();
        test_sensitive_and_tree_select_workflow();
        test_button_search_and_otp_contracts();
        test_layout_and_navigation_pages();
        test_remaining_workflows_and_drafts();
        test_complete_form_examples();
        test_readable_labels_and_sample_footers();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
