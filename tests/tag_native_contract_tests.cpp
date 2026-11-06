#include "component/tag_badge_component.hpp"
#include "component/form_component.hpp"
#include "component/popover_component.hpp"
#include "support/timeline_fixture.hpp"
#include <iostream>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool value, const char* message) {
    ++checks;
    check(value, message);
}

void tick(Fixture& f, std::int64_t us) {
    f.services.tick_animations(animation::AnimationTime::microseconds(us));
    sync(f);
}

void tag_close() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    bool cancel = true;
    int callbacks{};
    InputRef ref;
    Signal<bool> controlled{true};
    bool acknowledge = false;
    f.services.mount(Content{[&] {
        Tag(TagProps{}.closable().onClose(std::function<void(TagCloseEvent&)>{[&](TagCloseEvent& e) {
            ++callbacks;
            if (cancel) {
                e.preventDefault();
            }
        }}),
            TagContent{[&] { Input(InputProps{}.ref(ref).defaultValue(u8"retained")); }});
        Tag(TagProps{}.closable().visible(controlled).onClose([&] {
            if (acknowledge) {
                controlled.set(false);
            }
        }),
            TagContent{[] { Text(u8"Controlled"); }});
        CheckableTag(CheckableTagProps{}.defaultChecked(), TagContent{[] { Text(u8"Default check"); }});
    }});
    sync(f);
    const auto id = f.services.tag_badge().tags()[0];
    const auto other = f.services.tag_badge().tags()[1];
    const auto filter = f.services.tag_badge().tags()[2];
    const auto part = f.services.tag_badge().tag_snapshot(id);
    const auto duration =
        animation::resolve_motion_tokens(f.services.components().theme_scope(id)->snapshot()).fast.count_microseconds();
    tick(f, duration);
    expect(ref.focus(), "Tag Input did not focus");
    click(f, part.close);
    expect(callbacks == 1 && f.services.tag_badge().tag_snapshot(id).visible &&
               !f.services.tag_badge().tag_snapshot(id).closing,
           "Cancelled close changed accepted state");
    cancel = false;
    expect(ref.focus(), "Cancelled Tag editor did not regain focus");
    const auto stamp = f.inputs.sessions().active();
    expect(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {1, 0}, stamp})), "Tag preedit setup failed");
    const auto editor = f.inputs.mounted_inputs().front().editor;
    click(f, part.close);
    expect(!f.inputs.sessions().active().valid() && !f.inputs.editors().require(editor).composition().active &&
               !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp})),
           "Closing Tag retained preedit/session or accepted stale text");
    expect(f.services.tag_badge().tag_snapshot(id).closing && !ref.focus(), "Closing Tag retained input eligibility");
    tick(f, duration + duration / 2);
    expect(f.services.tag_badge().tag_snapshot(id).motion > 0 && f.services.tag_badge().tag_snapshot(id).motion < 1,
           "Tag exit had no finite intermediate state");
    tick(f, duration * 3);
    expect(!f.services.tag_badge().tag_snapshot(id).visible && ref.bound(),
           "Tag default close did not hide retained content at animation boundary");
    click(f, f.services.tag_badge().tag_snapshot(other).close);
    expect(f.services.tag_badge().tag_snapshot(other).visible && !f.services.tag_badge().tag_snapshot(other).closing,
           "Controlled close refusal mutated accepted state");
    acknowledge = true;
    click(f, f.services.tag_badge().tag_snapshot(other).close);
    tick(f, duration * 5);
    expect(!f.services.tag_badge().tag_snapshot(other).visible, "Controlled close acknowledgement failed");
    expect(f.services.tag_badge().tag_snapshot(filter).checked, "defaultChecked missing");
    click(f, filter);
    expect(!f.services.tag_badge().tag_snapshot(filter).checked, "Uncontrolled CheckableTag did not change");
    f.services.dispose();
    expect(!ref.bound() && !f.services.next_frame_deadline(), "Tag lifecycle leaked editor or timer");
}

void groups() {
    Fixture f;
    InputRef ref;
    int builds{};
    auto rich = std::make_shared<TagItemRenderer>();
    rich->render = [&] {
        ++builds;
        Input(InputProps{}.defaultValue(u8"draft").ref(ref));
    };
    const TagGroupItem a{u8"a", u8"A", TagColor{PresetColor::Blue}, false, false, rich};
    const TagGroupItem b{u8"b", u8"B"};
    const TagGroupItem c{u8"c", u8"C"};
    Signal<std::vector<TagGroupItem>> items{std::vector{a, b, c}};
    Signal<bool> disabled{false};
    bool accept = false;
    int requests{};
    f.services.mount(Content{[&] {
        TagGroup(TagGroupProps{}.items(items).draggable().disabled(disabled).onReorder([&](TagValues order) {
            ++requests;
            if (accept) {
                std::vector<TagGroupItem> next;
                for (const auto& key : order) {
                    for (const auto& row : items.get()) {
                        if (row.key == key) {
                            next.push_back(row);
                            break;
                        }
                    }
                }
                items.set(next);
            }
        }));
    }});
    sync(f);
    const auto id = f.services.tag_badge().groups()[0];
    const auto before = f.services.tag_badge().group_snapshot(id);
    expect(ref.focus() && ref.select(1, 4), "TagGroup rich editor focus failed");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    const auto original = std::string(f.inputs.editors().require(editor).value());
    const auto selection = f.inputs.editors().require(editor).selection();
    items.set(std::vector{c, a, b});
    sync(f);
    const auto moved = f.services.tag_badge().group_snapshot(id);
    expect(moved.items[1].second == before.items[0].second && builds == 1 && ref.bound() &&
               f.inputs.editors().require(editor).value() == original,
           "TagGroup keyed reorder cloned editor");
    expect(f.inputs.editors().require(editor).selection() == selection, "TagGroup reorder lost selection");
    const auto owner = f.services.components().parent(moved.items[0].second).value();
    const auto handle = f.services.components().children(owner).front();
    const auto hits = f.services.interactions().declaration_order();
    const auto find_hit = [&](runtime::ComponentId component) {
        for (auto h : hits) {
            if (f.services.interactions().require(h).component == component) {
                return h;
            }
        }
        return input::InteractionId{};
    };
    const auto hit = find_hit(handle);
    expect(f.services.focus().request_focus(hit, FocusModality::keyboard), "TagGroup handle focus failed");
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    expect(requests == 1 && f.services.tag_badge().group_snapshot(id).items[0].first == String{u8"c"},
           "Controlled reorder refusal mutated order");
    accept = true;
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    expect(requests == 2 && f.services.tag_badge().group_snapshot(id).items[1].first == String{u8"c"} && builds == 1,
           "Controlled reorder acknowledgement rebuilt stable content");
    const auto r = bounds(f, handle);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::move, PointerButton::none, r.x + 20, r.y + 5});
    expect(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "TagGroup drag did not capture");
    sync(f);
    expect(bounds(f, handle).x > r.x + 4, "TagGroup drag never moved actual retained geometry");
    disabled.set(true);
    sync(f);
    expect(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
               !f.services.tag_badge().group_snapshot(id).dragging,
           "Disabled group retained capture");
    disabled.set(false);
    sync(f);
    bool rejected = false;
    try {
        items.set(std::vector{a, a});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected && f.services.tag_badge().group_snapshot(id).items.size() == 3 && ref.bound(),
           "Invalid TagGroup update was not atomic");
    auto reentrant = std::make_shared<TagItemRenderer>();
    reentrant->render = [&] {
        items.set(std::vector{b, c});
        Text(u8"Reentrant update");
    };
    items.set(
        std::vector{a, b, c, TagGroupItem{u8"d", u8"D", TagColor{PresetColor::Default}, false, false, reentrant}});
    sync(f);
    expect(f.services.components().contains(id), "Reentrant Tag factory retired a coherent accepted group");
    expect(!ref.bound() && f.services.tag_badge().group_snapshot(id).items.size() == 2,
           "TagGroup remove orphaned rich key");
    f.services.dispose();
    expect(!f.services.next_frame_deadline() && f.services.interactions().size() == 0, "TagGroup cleanup leaked");
}

void factory_revision_preserves_sibling() {
    Fixture f;
    InputRef old_a;
    InputRef next_a;
    InputRef b_ref;
    int b_builds{};
    const auto a_renderer = std::make_shared<TagItemRenderer>();
    a_renderer->render = [&] {
        Input(InputProps{}.ref(old_a).defaultValue(u8"A draft"));
    };
    const auto b_renderer = std::make_shared<TagItemRenderer>();
    b_renderer->render = [&] {
        ++b_builds;
        Input(InputProps{}.ref(b_ref).defaultValue(u8"B draft"));
    };
    const TagGroupItem a{u8"a", u8"A", TagColor{PresetColor::Default}, false, false, a_renderer};
    const TagGroupItem b{u8"b", u8"B", TagColor{PresetColor::Blue}, false, false, b_renderer};
    Signal<std::vector<TagGroupItem>> items{std::vector{a, b}};
    f.services.mount(Content{[&] { TagGroup(TagGroupProps{}.items(items)); }});
    sync(f);
    const auto group = f.services.tag_badge().groups()[0];
    const auto initial = f.services.tag_badge().group_snapshot(group);
    const auto input = f.inputs.mounted_inputs()[1];
    expect(
        b_ref.focus({InputFocusCursor::All}) &&
            bool(f.inputs.dispatch(TextCommitted{String{u8"actual edited B draft"}, f.inputs.sessions().active()})) &&
            b_ref.select(2, 6),
        "Sibling draft/selection setup failed");
    const auto selection = f.inputs.editors().require(input.editor).selection();
    auto renamed_b = b;
    renamed_b.text = u8"Renamed B";
    items.set(std::vector{renamed_b, a});
    sync(f);
    expect(b_ref.bound() && b_builds == 1 &&
               f.inputs.editors().require(input.editor).value() == "actual edited B draft" &&
               f.inputs.editors().require(input.editor).selection() == selection,
           "Same-key data/reorder rebuilt sibling draft or selection");
    const auto replacement = std::make_shared<TagItemRenderer>();
    replacement->render = [&] {
        Input(InputProps{}.ref(next_a).defaultValue(u8"new A factory"));
    };
    auto revised_a = a;
    revised_a.content = replacement;
    items.set(std::vector{renamed_b, revised_a});
    sync(f);
    expect(!old_a.bound() && next_a.bound() && b_ref.bound() && b_builds == 1 &&
               f.services.tag_badge().group_snapshot(group).items[0].second == initial.items[1].second &&
               f.inputs.editors().require(input.editor).value() == "actual edited B draft" &&
               f.inputs.editors().require(input.editor).selection() == selection,
           "Local factory revision rebuilt unrelated key or lost its draft/selection");
    items.set(std::vector{renamed_b});
    sync(f);
    expect(!next_a.bound() && b_ref.bound() && b_builds == 1, "Removing revised key retired sibling editor");
    f.services.dispose();
}

void pointer_drop_geometry() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    f.services.mount(Content{[] {
        TagGroup(
            TagGroupProps{}
                .items(std::vector{TagGroupItem{u8"a", u8"A"}, TagGroupItem{u8"b", u8"B"}, TagGroupItem{u8"c", u8"C"}})
                .draggable());
    }});
    sync(f);
    const auto group = f.services.tag_badge().groups()[0];
    const auto initial = f.services.tag_badge().group_snapshot(group);
    const auto first_owner = *f.services.components().parent(initial.items[0].second);
    const auto handle = f.services.components().children(first_owner).front();
    const auto target_owner = *f.services.components().parent(initial.items[2].second);
    const auto from = bounds(f, handle);
    const auto target = bounds(f, target_owner);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   from.x + from.width / 2, from.y + from.height / 2});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                   target.x + target.width / 2, target.y + target.height / 2});
    sync(f);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary,
                                   target.x + target.width / 2, target.y + target.height / 2});
    sync(f);
    const auto moved = f.services.tag_badge().group_snapshot(group);
    expect(moved.items[2].first == String{u8"a"} && moved.items[1].first == String{u8"c"},
           "Pointer drop did not resolve the exact third visible row");
    const auto owner = *f.services.components().parent(moved.items[0].second);
    const auto next_handle = f.services.components().children(owner).front();
    const auto start = bounds(f, next_handle);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   start.x + start.width / 2, start.y + start.height / 2});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, 500, 500});
    sync(f);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 500, 500});
    sync(f);
    expect(f.services.tag_badge().group_snapshot(group).items == moved.items &&
               !f.services.pointer().state(PointerIdentity::mouse())->capture,
           "Dropping on empty clipped space reordered items or kept capture");
    f.services.dispose();
}

void close_focus_and_factory_failure() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    f.services.mount(Content{[] {
        TagGroup(TagGroupProps{}
                     .items(std::vector{TagGroupItem{u8"a", u8"A", TagColor{PresetColor::Default}, false, true},
                                        TagGroupItem{u8"b", u8"B", TagColor{PresetColor::Default}, false, true}})
                     .draggable());
    }});
    sync(f);
    const auto group = f.services.tag_badge().groups()[0];
    const auto view = f.services.tag_badge().group_snapshot(group);
    const auto first = f.services.tag_badge().tag_snapshot(view.items[0].second);
    click(f, first.close);
    const auto focused = f.services.focus().state().focused;
    const auto* hit = focused ? f.services.interactions().find(*focused) : nullptr;
    const auto next_owner = *f.services.components().parent(view.items[1].second);
    expect(hit && descendant(f, hit->component, next_owner), "Reduced motion group close lost roving focus");
    f.services.dispose();

    Fixture g;
    Signal<std::vector<TagGroupItem>> items{std::vector{TagGroupItem{u8"a", u8"A"}}};
    g.services.mount(Content{[&] {
        TagGroup(TagGroupProps{}.items(items));
        Button(ButtonProps{}, [] { Text(u8"Independent"); });
    }});
    sync(g);
    const auto owner = g.services.tag_badge().groups()[0];
    const auto sibling = g.buttons.mounted_buttons()[0].component;
    auto bad = std::make_shared<TagItemRenderer>();
    bad->render = [] {
        Input(InputProps{}.defaultValue(u8"must retire"));
        throw std::runtime_error("synthetic Tag factory failure");
    };
    bool threw = false;
    try {
        items.set(std::vector{TagGroupItem{u8"a", u8"A"},
                              TagGroupItem{u8"b", u8"B", TagColor{PresetColor::Default}, false, false, bad}});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    sync(g);
    expect(threw && !g.services.components().contains(owner) && g.services.components().contains(sibling) &&
               g.inputs.mounted_inputs().empty(),
           "Tag factory failure left partial children or retired sibling");
    g.services.dispose();
}

void form_scope_and_failures() {
    Fixture f;
    FormRef form;
    FormField<String> outer{String{u8"outer accepted"}};
    FormField<String> inner{String{u8"inner accepted"}};
    InputRef private_ref;
    InputRef public_ref;
    InputRef inner_ref;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                              Tag(TagProps{}, TagContent{[&] {
                                      Input(InputProps{}.defaultValue(u8"private draft").ref(private_ref));
                                      FormItem(FormItemProps{}.field(u8"inner", inner).noStyle(),
                                               FormItemContent{[&] { Input(InputProps{}.ref(inner_ref)); }});
                                  }});
                              Input(InputProps{}.ref(public_ref));
                          }});
             }});
    }});
    sync(f);
    const auto inputs = f.inputs.mounted_inputs();
    expect(inputs.size() == 3 && f.inputs.display_snapshot(inputs[0].component).text == "private draft" &&
               f.inputs.display_snapshot(inputs[1].component).text == "inner accepted" &&
               f.inputs.display_snapshot(inputs[2].component).text == "outer accepted",
           "Tag private content stole Form binding or suppressed explicit nested FormItem");
    expect(public_ref.focus({InputFocusCursor::All}), "Public Form editor focus failed");
    f.inputs.dispatch(TextCommitted{String{u8"outer edit"}, f.inputs.sessions().active()});
    sync(f);
    expect(outer.get() == String{u8"outer edit"}, "Public Form authority after Tag was lost");
    for (int n = 0; n < 3; ++n) {
        expect(form.reset(), "Repeated Form reset failed");
        sync(f);
    }
    expect(f.inputs.display_snapshot(inputs[0].component).text == "private draft" &&
               outer.get() == String{u8"outer accepted"},
           "Form reset touched private Tag draft");
    f.services.dispose();
    Fixture h;
    runtime::ComponentId self;
    h.services.mount(Content{[&] {
        Tag(TagProps{}.closable().onClose([&] { h.services.destroy(self); }),
            TagContent{[] { Text(u8"self destroy"); }});
    }});
    sync(h);
    self = h.services.tag_badge().tags()[0];
    const auto close = h.services.tag_badge().tag_snapshot(self).close;
    click(h, close);
    expect(!h.services.components().contains(self) && h.services.interactions().size() == 0,
           "Tag self-destroy callback leaked interaction");
    Fixture g;
    g.services.mount(Content{[&] {
        Tag(TagProps{}.closable().onClose([] { throw std::runtime_error("synthetic close failure"); }),
            TagContent{[] { Text(u8"throw"); }});
    }});
    sync(g);
    const auto failed = g.services.tag_badge().tags()[0];
    bool threw = false;
    try {
        click(g, g.services.tag_badge().tag_snapshot(failed).close);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw && !g.services.components().contains(failed) && g.services.interactions().size() == 0 &&
               !g.services.next_frame_deadline(),
           "Throwing Tag callback did not retire exact owner");
}
} // namespace

int main() {
    try {
        tag_close();
        groups();
        factory_revision_preserves_sibling();
        pointer_drop_geometry();
        close_focus_and_factory_failure();
        form_scope_and_failures();
        std::cout << checks << " Tag native checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
