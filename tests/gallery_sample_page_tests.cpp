#include "gallery_sample_registry.hpp"
#include "gallery_page_controller.hpp"
#include "component/otp_component.hpp"
#include "component/tree_select_component.hpp"
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

    Fixture() {
        surfaces.mount(Content{[this] {
                           Theme(ThemeProps{}.config(theme),
                                 ThemeContent{[] { Flex(FlexProps{}.vertical(true).align(FlexAlign::Start), [] {}); }});
                       }},
                       &inputs);
        const auto holder = services.components().root_components().front();
        pages = std::make_unique<GalleryPageController>(
            surfaces, model, frames, holder, [this](const GalleryPageRoute& route, const GalleryPageDraft& draft) {
                active = make_gallery_sample_page(route, draft);
                active.set_width(760);
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
        require(services.focus().request_focus(input.interaction, input::FocusModality::keyboard),
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
            const bool selected =
                descriptor.component_identity == route.component_identity && descriptor.family == route.family;
            require(active.sample_runs(descriptor.identity) == (selected ? 1 : 0), "unrelated page factory ran");
            expected += selected;
        }
        require(surfaces.mounted_surfaces().size() == expected, "inactive page surfaces remained alive");
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

void test_registry_and_factory_isolation() {
    Fixture f;
    std::set<std::string_view> identities;
    for (const auto& descriptor : gallery_samples()) {
        require(identities.insert(descriptor.identity).second && !descriptor.source_function.empty() &&
                    descriptor.public_header.starts_with("include/ryn/") &&
                    f.model.valid(f.model.find(descriptor.component_identity)->route(descriptor.family)),
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
            const bool active =
                descriptor.component_identity == route.component_identity && descriptor.family == route.family;
            require(f.active.sample_runs(descriptor.identity) == (active ? 1 : 0), "unrelated factory ran");
            expected += active;
        }
        require(f.surfaces.mounted_surfaces().size() == expected, "inactive sample surfaces remained mounted");
        require(f.active.telemetry().content_runs == 1, "page content was reconstructed during layout");
    }
    require(f.inputs.mounted_inputs().empty() && f.buttons.mounted_buttons().empty(), "Upload mounted a fake demo");
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
    require(f.services.tree_select().mounted().size() == 2 && f.inputs.mounted_inputs().size() == 3,
            "TreeSelect selector/note/Form workflow was lost");
    runtime::ComponentId target;
    runtime::ComponentId field;
    const auto form = f.services.form().mounted().front();
    for (const auto id : f.services.tree_select().mounted()) {
        (f.within(id, form) ? field : target) = id;
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
    f.show("ant.component.dropdown");
    const auto dropdown = f.active.target("gallery.dropdown.actions");
    f.action("gallery.sample.dropdown.actions", "gallery.dropdown.actions");
    require(f.services.dropdown().snapshot(dropdown).geometry.visible, "Dropdown command trigger did not open");
    f.key(input::Key::escape);
    require(!f.services.dropdown().snapshot(dropdown).geometry.visible, "Dropdown Escape did not close");
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

} // namespace

int main() {
    try {
        test_registry_and_factory_isolation();
        test_input_semantics_and_draft_round_trip();
        test_sensitive_and_tree_select_workflow();
        test_button_search_and_otp_contracts();
        test_layout_and_navigation_pages();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
