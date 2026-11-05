#include "support/data_display_fixture.hpp"

namespace {
using namespace data_display_test;

DescriptionsItem plain(String key) {
    return {key, DescriptionsLabel{[key] { Text(key); }}, DescriptionsContent{[] { Text(u8"Value"); }}};
}

void keyed_responsive_drafts() {
    Fixture f;
    Signal<int> topology{0};
    Signal<int> revision{0};
    Signal<int> span{1};
    Signal<bool> rtl{false};
    Signal<DescriptionsLocale> locale{DescriptionsLocale{}};
    Signal<LogicalLength> width{dp(600)};
    InputRef ref;
    int labels{};
    int contents{};
    f.services.mount(Content{[&] {
        Descriptions(
            DescriptionsProps{}.column(Responsive<int>{1}.at(Breakpoint::Sm, 3)).bordered().rtl(rtl).locale(locale),
            bind([&] {
                std::vector<DescriptionsItem> items;
                auto draft = DescriptionsItem{u8"draft", DescriptionsLabel{[&] {
                                                  ++labels;
                                                  Text(u8"Label");
                                              }},
                                              DescriptionsContent{[&] {
                                                  ++contents;
                                                  Input(InputProps{}.ref(ref));
                                              }}}
                                 .span(span)
                                 .labelRevision(static_cast<std::uint64_t>(revision.get()));
                if (topology.get() == 1) {
                    items.push_back(plain(u8"new"));
                }
                items.push_back(draft);
                items.push_back(plain(u8"fill").fill());
                items.push_back(plain(u8"last"));
                if (topology.get() == 3) {
                    items.push_back(draft);
                }
                return items;
            }));
    }});
    sync(f);
    const auto id = f.services.descriptions().mounted()[0];
    const auto original = f.services.descriptions().snapshot(id);
    check(original.rows.size() == 2 && original.rows[0].back().span == 2, "fill did not consume the remaining row");
    draft(f, ref, u8"native cell draft");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    check(ref.focus({InputFocusCursor::All}), "retained editor selection focus failed");
    const auto selection = f.inputs.editors().require(editor).selection();
    topology.set(1);
    sync(f);
    check(f.inputs.editors().require(editor).selection() == selection, "keyed move changed native Input selection");
    check(f.services.descriptions().snapshot(id).items[1].content == original.items[0].content &&
              f.inputs.mounted_inputs()[0].editor == editor,
          "responsive keyed reconciliation rebuilt editor");
    revision.set(1);
    sync(f);
    check(labels == 2 && contents == 1, "label revision remounted content");
    rtl.set(true);
    sync(f);
    const auto current = f.services.descriptions().snapshot(id);
    check(bounds(f, current.items[0].viewport).x > bounds(f, current.items[1].viewport).x &&
              bounds(f, current.items[1].label).x > bounds(f, current.items[1].content).x,
          "RTL failed to mirror cells and label/content allocation");
    sync(f, 300);
    check(f.services.descriptions().snapshot(id).rows.size() == 4 && contents == 1,
          "window logical breakpoint did not repack retained cells");
    bool rejected{};
    try {
        span.set(0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.descriptions().snapshot(id).rows.size() == 4, "invalid span partially published rows");
    span.set(1);
    sync(f);
    rejected = false;
    try {
        topology.set(3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.descriptions().snapshot(id).items.size() == 4, "duplicate topology mutated cells");
    topology.set(1);
    locale.set({String{u8"："}});
    width.set(dp(0));
    sync(f);
    check(contents == 1 && f.inputs.mounted_inputs()[0].editor == editor, "locale/zero geometry remounted editor");
}

void header_and_failure() {
    Fixture f;
    Signal<int> header{0};
    Signal<bool> fail{false};
    int content_runs{};
    int actions{};
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}
                         .titleContent(bind([&] {
                             const int n = header.get();
                             return DescriptionsTitle{
                                 [n] { Text(n == 0 ? String{u8"Original"} : String{u8"Changed"}); }};
                         }))
                         .extra(DescriptionsExtra{
                             [&] { Button(ButtonProps{}.onClick([&] { ++actions; }), [] { Text(u8"Save"); }); }}),
                     bind([&] {
                         std::vector<DescriptionsItem> items{
                             DescriptionsItem{u8"a", DescriptionsLabel{[] { Text(u8"A"); }}, DescriptionsContent{[&] {
                                                  ++content_runs;
                                                  Input(InputProps{});
                                              }}}};
                         if (fail.get()) {
                             items.push_back({u8"bad", DescriptionsLabel{[] { Text(u8"bad"); }},
                                              DescriptionsContent{[] { throw std::runtime_error("cell"); }}});
                         }
                         return items;
                     }));
    }});
    sync(f);
    const auto button = f.buttons.mounted_buttons()[0].component;
    header.set(1);
    sync(f);
    click(f, button);
    check(actions == 1 && content_runs == 1 && f.buttons.mounted_buttons()[0].component == button,
          "header replacement remounted rich header action or cells");
    bool threw{};
    try {
        fail.set(true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && f.services.descriptions().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              f.buttons.mounted_buttons().empty(),
          "failed cell append orphaned regions");
}

void mount_time_header_authority() {
    Fixture f;
    Signal<int> generation{0};
    int factories{};
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}.titleContent(bind([&] {
            const int value = generation.get();
            return DescriptionsTitle{[&, value] {
                ++factories;
                Text(value == 0 ? String{u8"stale header"} : String{u8"current header"});
                if (value == 0) {
                    generation.set(1);
                }
            }};
        })),
                     {});
    }});
    sync(f);
    check(factories == 2, "mount-time header authority was ignored or mounted redundantly");
    bool current{};
    bool stale{};
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto value = f.scene.text_state(text.scene).content();
        current |= value.bytes() == "current header";
        stale |= value.bytes() == "stale header";
    }
    check(current && !stale, "old header survived mount-time source replacement");
}

void replaced_cell_theme() {
    Fixture f;
    Signal<int> revision{0};
    ThemeConfig config;
    const auto label = Color::rgba8(140, 40, 90);
    const auto content = Color::rgba8(30, 100, 140);
    config.descriptions.tokens.label = label;
    config.descriptions.tokens.content = content;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Descriptions(DescriptionsProps{}, bind([&] {
                                   return std::vector<DescriptionsItem>{
                                       DescriptionsItem{u8"a", DescriptionsLabel{[] { Text(u8"styled label"); }},
                                                        DescriptionsContent{[] { Text(u8"styled content"); }}}
                                           .labelRevision(static_cast<std::uint64_t>(revision.get()))
                                           .contentRevision(static_cast<std::uint64_t>(revision.get()))};
                               }));
              }});
    }});
    sync(f);
    revision.set(1);
    sync(f);
    check(text_color(f, "styled label") == channels(label) && text_color(f, "styled content") == channels(content),
          "replaced cells lost distinct label/content Theme styles");
}

void self_destroying_predicate() {
    Fixture f;
    Signal<int> topology{0};
    bool retire{};
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}, bind([&] {
                         static_cast<void>(topology.get());
                         return std::vector<DescriptionsItem>{DescriptionsItem{
                             u8"a", DescriptionsLabel{[] { Text(u8"A"); }}, DescriptionsContent{[] {
                                 Text(u8"Body");
                             }}}.visible(bind([&] {
                             if (retire) {
                                 static_cast<void>(f.services.destroy(owner));
                             }
                             return true;
                         }))};
                     }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.descriptions().mounted()[0];
    sync(f);
    retire = true;
    topology.set(1);
    sync(f);
    check(!f.services.components().contains(owner) && f.buttons.mounted_buttons().size() == 1,
          "predicate retirement left or accessed a dead owner");
}

void self_destroying_factory() {
    Fixture f;
    Signal<bool> remove{false};
    runtime::ComponentId owner;
    const auto body = [&] {
        Input(InputProps{});
        if (remove.get()) {
            check(f.services.destroy(owner), "factory could not destroy owner");
        }
    };
    f.services.mount(Content{[&] {
        Descriptions(DescriptionsProps{}, bind([&] {
                         std::vector<DescriptionsItem> rows{
                             {u8"safe", DescriptionsLabel{[] { Text(u8"Safe"); }}, DescriptionsContent{[] {}}}};
                         if (remove.get()) {
                             rows.push_back({u8"retire", DescriptionsLabel{[] {}}, DescriptionsContent{body}});
                         }
                         return rows;
                     }));
        Button(ButtonProps{}, [] { Text(u8"Sibling"); });
    }});
    owner = f.services.descriptions().mounted()[0];
    sync(f);
    try {
        remove.set(true);
    } catch (const std::exception&) {
    }
    sync(f);
    check(!f.services.components().contains(owner) && f.inputs.mounted_inputs().empty() &&
              f.buttons.mounted_buttons().size() == 1,
          "self-destroying factory left orphan input or removed sibling");
}

} // namespace

int main() {
    try {
        form_composition(2);
        self_destroying_factory();
        self_destroying_predicate();
        keyed_responsive_drafts();
        header_and_failure();
        mount_time_header_authority();
        replaced_cell_theme();
        std::cout << "Descriptions: 2 new responsive/retained/header/failure scenarios passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
