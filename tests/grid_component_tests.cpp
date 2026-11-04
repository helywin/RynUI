#include "component/grid_component.hpp"
#include "component/responsive_layout.hpp"
#include "support/input_fixture.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float first, float second) {
    return std::abs(first - second) < 0.02F;
}

runtime::ComponentId leaf(Fixture& f, float width, float height) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::LeafLayout{{width, height}});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    return id;
}

runtime::ComponentId root(Fixture& f) {
    return f.services.components().root_components().front();
}

void breakpoint_contract() {
    Breakpoints points;
    check(points.width(Breakpoint::Xs) == 480 && points.width(Breakpoint::Xxxl) == 1920, "default breakpoints drifted");
    Responsive<int> value{1};
    value.at(Breakpoint::Xs, 2).at(Breakpoint::Md, 3).at(Breakpoint::Xxxl, 4);
    check(value.resolve(0) == 2 && value.resolve(767) == 2 && value.resolve(768) == 3 && value.resolve(1920) == 4,
          "responsive threshold/default matching failed");
    bool rejected{};
    try {
        Breakpoints invalid{{0, 1, 2, 2, 3, 4, 5}};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "duplicate breakpoint accepted");
    rejected = false;
    try {
        static_cast<void>(points.matches(Breakpoint::Md, std::numeric_limits<float>::infinity()));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "non-finite window width accepted");
}

void spans_offsets_and_gutters() {
    Fixture f;
    f.services.mount(Content{[&] {
        Row(RowProps{}.gutter(dp(24), dp(12)).layout(LayoutStyle{}.width(dp(456))), RowContent{[&] {
                Col(ColProps{}.span(8), ColContent{[&] { leaf(f, 10, 20); }});
                Col(ColProps{}.span(8).offset(8), ColContent{[&] { leaf(f, 10, 30); }});
                Col(ColProps{}.span(12).push(2).pull(1), ColContent{[&] { leaf(f, 10, 40); }});
            }});
    }});
    f.synchronize(1000);
    const auto values = detail::grid_columns(f.services.components(), root(f));
    check(near(values[0].bounds.width, 160) && near(values[0].bounds.x, -12), "24-column width/gutter failed");
    check(near(values[1].bounds.x, 308) && near(values[1].bounds.y, 0), "column offset changed span geometry");
    check(near(values[2].bounds.x, 8) && near(values[2].bounds.y, 42) && near(values[2].bounds.width, 240),
          "wrap/vertical gutter/push-pull failed");
    const auto node = f.services.components().root(values[0].component);
    const auto content = f.nodes.require(node).children.front();
    check(near(f.nodes.require(content).bounds.x, 0), "half gutter padding did not align visible content");
}

void responsive_identity_and_hidden_input() {
    Fixture f;
    Signal<Responsive<ColOptions>> options{Responsive<ColOptions>{ColOptions{.span = 24, .order = 3}}
                                               .at(Breakpoint::Md, ColOptions{.span = 12, .offset = 2})
                                               .at(Breakpoint::Lg, ColOptions{.order = -1})
                                               .at(Breakpoint::Xl, ColOptions{.span = 0})};
    int mounts{};
    f.services.mount(Content{[&] {
        Row(RowProps{}.layout(LayoutStyle{}.width(dp(240))), RowContent{[&] {
                Col(ColProps{}.push(1).responsive(options), ColContent{[&] {
                        ++mounts;
                        Button(ButtonProps{}, [] { Text(u8"保留"); });
                        Input(InputProps{}.defaultValue(u8"draft"));
                    }});
            }});
    }});
    auto values = detail::grid_columns(f.services.components(), root(f));
    f.synchronize(1000);
    values = detail::grid_columns(f.services.components(), root(f));
    const auto stable = values[0].component;
    const auto input = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(input.interaction, input::FocusModality::keyboard), "input focus failed");
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"edited"}, f.inputs.sessions().active()})),
          "Grid draft edit failed");
    const auto draft = std::string{f.inputs.display_snapshot(input.component).text};
    check(values[0].options.span == 12 && values[0].options.offset == 2 && values[0].options.order == -1 &&
              values[0].options.push == 1 && near(values[0].bounds.width, 120),
          "per-field window breakpoint merge failed");
    const auto interaction = f.buttons.mounted_buttons().front().interaction;
    check(f.services.focus().request_focus(interaction, input::FocusModality::keyboard), "button focus failed");
    f.synchronize(1200);
    values = detail::grid_columns(f.services.components(), root(f));
    check(values[0].hidden && !f.services.components().branch_active(stable) &&
              !f.services.focus().request_focus(interaction, input::FocusModality::keyboard) &&
              !f.services.focus().state().focused,
          "hidden column remained focusable");
    f.synchronize(500);
    values = detail::grid_columns(f.services.components(), root(f));
    check(!values[0].hidden && values[0].options.span == 24 && values[0].options.offset == 0 &&
              values[0].component == stable && mounts == 1,
          "responsive layout remounted or leaked wider overrides");
    check(f.inputs.mounted_inputs().front().component == input.component &&
              f.inputs.display_snapshot(input.component).text == draft,
          "Grid reflow/hidden restore lost the edited input value");
    options.set(Responsive<ColOptions>{ColOptions{.span = 8}}.at(Breakpoint::Sm, ColOptions{.order = 2}));
    f.synchronize(600);
    check(detail::grid_columns(f.services.components(), root(f))[0].options.span == 8 && mounts == 1,
          "reactive responsive options did not remeasure retained content");
}

void flex_distribution_and_constraints() {
    Fixture f;
    f.services.mount(Content{[&] {
        Row(RowProps{}.wrap(false).layout(LayoutStyle{}.width(dp(600))), RowContent{[&] {
                Col(ColProps{}.flex(GridFlex::fixed(dp(100))), ColContent{[&] { leaf(f, 20, 10); }});
                Col(ColProps{}.flex(GridFlex::fill()).layout(LayoutStyle{}.max_width(dp(120))),
                    ColContent{[&] { leaf(f, 20, 20); }});
                Col(ColProps{}.flex(GridFlex::fill(2)), ColContent{[&] { leaf(f, 20, 30); }});
            }});
    }});
    f.synchronize(1000);
    auto values = detail::grid_columns(f.services.components(), root(f));
    check(near(values[0].bounds.width, 100) && near(values[1].bounds.width, 120) && near(values[2].bounds.width, 380),
          "typed grow/basis/max-width freeze failed");
    Fixture shrink;
    shrink.services.mount(Content{[&] {
        Row(RowProps{}.wrap(false).layout(LayoutStyle{}.width(dp(200))), RowContent{[&] {
                for (int index = 0; index < 2; ++index) {
                    Col(ColProps{}.flex(GridFlex{1, 1, dp(200)}), ColContent{[&] { leaf(shrink, 10, 10); }});
                }
            }});
    }});
    shrink.synchronize();
    values = detail::grid_columns(shrink.services.components(), root(shrink));
    check(near(values[0].bounds.width, 100) && near(values[1].bounds.width, 100), "typed flex shrink failed");
}

void alignment_rtl_and_order() {
    for (const auto justify : {RowJustify::Start, RowJustify::End, RowJustify::Center, RowJustify::SpaceAround,
                               RowJustify::SpaceBetween, RowJustify::SpaceEvenly}) {
        Fixture f;
        Signal<FlexDirection> direction{FlexDirection::LeftToRight};
        Signal<Responsive<RowAlign>> align{Responsive<RowAlign>{RowAlign::Bottom}};
        f.services.mount(Content{[&] {
            Row(RowProps{}.justify(justify).align(align).direction(direction).layout(
                    LayoutStyle{}.width(dp(240)).height(dp(80))),
                RowContent{[&] {
                    Col(ColProps{}.span(6).order(1).layout(LayoutStyle{}.max_height(dp(50))),
                        ColContent{[&] { leaf(f, 10, 20); }});
                    Col(ColProps{}.span(6).order(-1), ColContent{[&] { leaf(f, 10, 40); }});
                }});
        }});
        f.synchronize(1000);
        auto values = detail::grid_columns(f.services.components(), root(f));
        check(values[0].bounds.x > values[1].bounds.x && near(values[0].bounds.y, 60) && near(values[1].bounds.y, 40),
              "visual order/bottom alignment failed");
        const auto first_x = values[0].bounds.x;
        direction.set(FlexDirection::RightToLeft);
        align.set(Responsive<RowAlign>{RowAlign::Middle});
        f.synchronize(1000);
        values = detail::grid_columns(f.services.components(), root(f));
        check(near(values[0].bounds.x, 240 - first_x - 60) && near(values[0].bounds.y, 30), "RTL/middle failed");
        align.set(Responsive<RowAlign>{RowAlign::Stretch});
        f.synchronize(1000);
        values = detail::grid_columns(f.services.components(), root(f));
        check(near(values[0].bounds.height, 50) && near(values[1].bounds.height, 80),
              "stretch ignored the external max-height constraint");
    }
}

void validation_and_lifecycle() {
    Fixture f;
    Signal<int> span{12};
    Signal<GridGutter> gutter{GridGutter{}};
    f.services.mount(Content{
        [&] { Row(RowProps{}.gutter(gutter), RowContent{[&] { Col(ColProps{}.span(span), ColContent{[] {}}); }}); }});
    f.synchronize();
    bool rejected{};
    try {
        span.set(25);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && detail::grid_columns(f.services.components(), root(f))[0].options.span == 12,
          "invalid span changed committed configuration");
    rejected = false;
    try {
        gutter.set(GridGutter{Responsive<LogicalLength>{dp(-1)}, Responsive<LogicalLength>{dp(0)}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "negative gutter accepted");
    check(f.services.destroy(root(f)), "Grid destroy failed");
    f.dirty.clear();
    static_cast<void>(f.frames.consume_request());
    span.set(8);
    gutter.set(GridGutter{});
    f.services.responsive_layout().set_window_width(1600);
    check(f.nodes.size() == 0 && !f.frames.pending(), "destroyed Grid retained subscriptions/resources");
    Fixture invalid;
    rejected = false;
    try {
        invalid.services.mount(Content{[] { Row(RowProps{}, RowContent{[] { Text(u8"invalid"); }}); }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && invalid.nodes.size() == 0, "invalid Row topology did not roll back");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "breakpoints") {
            breakpoint_contract();
        } else if (name == "spans") {
            spans_offsets_and_gutters();
        } else if (name == "responsive") {
            responsive_identity_and_hidden_input();
        } else if (name == "flex") {
            flex_distribution_and_constraints();
        } else if (name == "alignment") {
            alignment_rtl_and_order();
        } else if (name == "lifecycle") {
            validation_and_lifecycle();
        } else {
            throw std::runtime_error("unknown Grid test case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
