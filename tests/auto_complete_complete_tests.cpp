#include "support/auto_complete_fixture.hpp"
#include "component/auto_complete_model.hpp"
#include "component/option_list_support.hpp"
#include "runtime/component_host.hpp"
#include <ryn/flex.hpp>
#include <thread>
#include <mutex>

namespace {
using namespace auto_complete_test;

void window_and_groups() {
    std::vector<AutoCompleteOption> many;
    for (int i = 0; i < 10000; ++i) {
        const auto key = String::from_utf8(std::to_string(i)).value();
        many.push_back({key, key, key});
    }
    AutoCompleteRef ref;
    Fixture f;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}
                         .options(many)
                         .ref(ref)
                         .defaultOpen()
                         .listHeight(64.0F)
                         .direction(TextDirection::RightToLeft)
                         .popupWidth(300.0F)
                         .layout(LayoutStyle{}.width(dp(200))));
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    auto s = snapshot(f, id);
    check(s.rows.size() <= 6 && s.total_rows == 10000 && s.bounds.width == 300, "window did not bound realized rows");
    check(ref.focus(), "window focus");
    for (int n = 0; n < 80; ++n) {
        key(f, Key::down);
    }
    s = snapshot(f, id);
    check(s.active && *s.active >= 79 && s.scroll > 0 && s.rows.size() <= 6, "window keyboard reveal");
    const auto found = std::ranges::find(s.row_indices, *s.active);
    check(found != s.row_indices.end(), "active option not realized");
    const auto rect = bounds(f, s.rows[static_cast<std::size_t>(found - s.row_indices.begin())]);
    check(rect.y >= s.clip.y && rect.y + rect.height <= s.clip.y + s.clip.height, "active option outside viewport");
    const auto count = f.services.components().component_count();
    for (int n = 0; n < 50; ++n) {
        const auto now = snapshot(f, id);
        check(f.inputs.dispatch(ScrollInputEvent{0, -1, now.clip.x + 4, now.clip.y + 4}), "window scroll");
        sync(f);
        check(snapshot(f, id).rows.size() <= 6 && f.services.components().component_count() <= count + 4,
              "window ordinary rows grew with visits");
    }
    clean(f);
    Signal<std::vector<AutoCompleteOptionGroup>> groups{std::vector<AutoCompleteOptionGroup>{
        {u8"g1", u8"Cities", {{u8"a", u8"Paris", u8"Paris"}, {u8"b", u8"Perth", u8"Perth"}}},
        {u8"g2", u8"People", {{u8"c", u8"Alice", u8"Alice"}}}}};
    Fixture g;
    g.services.mount(Content{
        [&] { AutoComplete(AutoCompleteProps{}.groups(groups).ref(ref).filterOption(AutoCompleteFilter{true})); }});
    sync(g);
    const auto grouped = g.services.auto_complete().mounted()[0];
    check(ref.focus(), "group focus");
    edit(g, u8"P");
    auto model = snapshot(g, grouped);
    check(model.active == 1 && model.total_rows == 3, "group filtering/header navigation");
    key(g, Key::down);
    check(snapshot(g, grouped).active == 2, "group header was selectable");
    key(g, Key::enter);
    check(snapshot(g, grouped).value == String{u8"Perth"}, "group choose");
    clean(g);
    Fixture rich;
    InputRef rich_input;
    int rich_runs = 0;
    AutoCompleteRichLabel label{AutoCompleteOptionLabel{[&] {
        ++rich_runs;
        Input(InputProps{}.ref(rich_input).defaultValue(u8"window draft"));
    }}};
    std::vector<AutoCompleteOption> rich_options{{u8"rich", u8"Rich", u8"Rich", false, label}};
    for (int i = 1; i < 100; ++i) {
        const auto value = String::from_utf8(std::to_string(i)).value();
        rich_options.push_back({value, value, value});
    }
    rich.services.mount(Content{
        [&] { AutoComplete(AutoCompleteProps{}.options(rich_options).ref(ref).defaultOpen().listHeight(64.0F)); }});
    sync(rich);
    const auto owner = rich.services.auto_complete().mounted()[0];
    const auto row = snapshot(rich, owner).rows[0];
    check(rich_input.focus(), "virtual rich focus");
    edit(rich, u8"saved virtual draft");
    const auto stamp = rich.inputs.sessions().active();
    check(bool(rich.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "virtual rich preedit");
    auto clip = snapshot(rich, owner).clip;
    check(rich.inputs.dispatch(ScrollInputEvent{0, -20, clip.x + 4, clip.y + 4}), "virtual rich scroll");
    sync(rich);
    check(!rich.inputs.editors().require(stamp.owner).composition().active &&
              !rich.services.components().branch_active(row),
          "offscreen retained AutoComplete row kept IME or activity");
    clip = snapshot(rich, owner).clip;
    check(rich.inputs.dispatch(ScrollInputEvent{0, 20, clip.x + 4, clip.y + 4}), "virtual rich return scroll");
    sync(rich);
    check(snapshot(rich, owner).rows[0] == row && rich_runs == 1 && rich_input.bound() &&
              rich.inputs.display_snapshot(rich.inputs.mounted_inputs()[1].component).text == "saved virtual draft",
          "AutoComplete virtual return replaced keyed rich draft");
    clean(rich);
}

void async_cancel_reentry() {
    auto session = std::make_unique<detail::AsyncOptionsSession<AutoCompleteOption>>();
    std::vector<String> queries;
    std::vector<AutoCompleteSource::Complete> completions;
    std::vector<String> applied;
    const auto apply = [&](AsyncOptionsResult<AutoCompleteOption> result) {
        applied.push_back(result.error);
    };
    AutoCompleteSource latest{[&](AsyncOptionsRequest request, auto complete) {
                                  queries.push_back(request.query);
                                  completions.push_back(std::move(complete));
                                  return AsyncOptionsCancel{};
                              },
                              [](auto task) { task(); }};
    AutoCompleteSource first{[&](AsyncOptionsRequest, auto) {
                                 return [&] {
                                     session->start(latest, u8"newer", apply);
                                 };
                             },
                             [](auto task) { task(); }};
    session->start(first, u8"first", apply);
    session->start(latest, u8"superseded", apply);
    check(queries == std::vector<String>{u8"newer"}, "cancel reentry replaced newer request");
    completions[0]({{}, u8"latest"});
    completions[0]({{}, u8"duplicate"});
    check(applied == std::vector<String>{u8"latest"}, "cancel reentry lost completion/duplicate guard");
    AutoCompleteSource destroy{[&](AsyncOptionsRequest, auto) {
                                   return [&] {
                                       session.reset();
                                   };
                               },
                               [](auto task) { task(); }};
    session->start(destroy, u8"destroy", apply);
    session->start(latest, u8"after destruction", apply);
    check(!session && queries.size() == 1, "destroy during cancellation started another provider");
}

void async_authority() {
    async_cancel_reentry();
    std::vector<AutoCompleteSource::Complete> requests;
    std::vector<std::function<void()>> posted;
    std::vector<std::shared_ptr<const std::atomic_bool>> cancelled;
    int cancel_count = 0;
    std::mutex mutex;
    AutoCompleteSource source{[&](AsyncOptionsRequest request, auto complete) {
                                  requests.push_back(std::move(complete));
                                  cancelled.push_back(request.cancelled);
                                  return [&] {
                                      ++cancel_count;
                                  };
                              },
                              [&](auto task) {
                                  std::scoped_lock lock(mutex);
                                  posted.push_back(std::move(task));
                              }};
    AutoCompleteRef ref;
    Fixture f;
    f.services.mount(Content{[&] { AutoComplete(AutoCompleteProps{}.source(source).ref(ref)); }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    check(ref.focus(), "async focus");
    edit(f, u8"old");
    edit(f, u8"new");
    check(requests.size() == 2 && cancel_count == 1 && cancelled[0]->load() && snapshot(f, id).pending,
          "superseded query did not cancel");
    std::thread worker([&] {
        requests[0]({{{u8"old", u8"old", u8"Old"}}, {}});
        requests[1]({{{u8"new", u8"new", u8"New"}}, {}});
    });
    worker.join();
    check(snapshot(f, id).rows.empty() && posted.size() == 1, "worker mutated Core or stale result was posted");
    auto apply = std::move(posted.back());
    posted.clear();
    apply();
    sync(f);
    check(snapshot(f, id).rows.size() == 1 && !snapshot(f, id).pending && snapshot(f, id).query == String{u8"new"},
          "latest owner-thread result missing");
    edit(f, u8"late");
    requests.back()({{{u8"late", u8"late", u8"Late"}}, {}});
    key(f, Key::escape);
    apply = std::move(posted.back());
    posted.clear();
    apply();
    sync(f);
    check(snapshot(f, id).total_rows == 1 && !snapshot(f, id).visible && cancelled.back()->load(),
          "close accepted late async result");
    check(ref.focus(), "async refocus");
    edit(f, u8"destroy");
    requests.back()({{{u8"after", u8"after", u8"After"}}, {}});
    clean(f);
    for (auto& task : posted) {
        task();
    }
    check(cancelled.back()->load() && !ref.bound(), "destroy did not invalidate queued completion");
    posted.clear();
    requests.clear();
    cancelled.clear();
    cancel_count = 0;
    Fixture g;
    g.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}
                         .ref(ref)
                         .source(source)
                         .options({{u8"waiting", u8"", u8"Synthetic request waiting", true}})
                         .popupRender(AutoCompletePopupRenderer{[&](std::function<void()> origin) {
                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                      origin();
                                      Button(ButtonProps{}.onClick([&] {
                                          requests.back()({{{u8"chosen", u8"footer-value", u8"Delivered"}}, {}});
                                          auto tasks = std::move(posted);
                                          posted.clear();
                                          for (auto& task : tasks) {
                                              task();
                                          }
                                      }),
                                             ButtonContent{[] { Text(u8"Deliver synthetic request"); }});
                                  }});
                         }}));
    }});
    sync(g);
    const auto owner = g.services.auto_complete().mounted()[0];
    check(ref.focus(), "footer delivery focus");
    edit(g, u8"footer");
    click(g, g.buttons.mounted_buttons()[0].component);
    check(cancel_count == 0 && !snapshot(g, owner).pending && snapshot(g, owner).visible &&
              snapshot(g, owner).rows.size() == 1,
          "popup delivery lost request to blur cancellation");
    click(g, snapshot(g, owner).rows[0]);
    check(snapshot(g, owner).value == String{u8"footer-value"} &&
              g.services.focus().state().focused == g.inputs.mounted_inputs()[0].interaction,
          "choice from popup footer did not return focus to native editor");
    clean(g);
}

void renderers_and_motion() {
    AutoCompleteRef ref;
    InputRef popup_input;
    Signal<AutoCompleteEditor> editor{AutoCompleteEditor{}};
    int editor_runs = 0;
    int popup_runs = 0;
    Signal<AutoCompletePopupRenderer> popup{AutoCompletePopupRenderer{[&](std::function<void()> origin) {
        ++popup_runs;
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 origin();
                 Input(InputProps{}.ref(popup_input).defaultValue(u8"draft"));
             }});
    }}};
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}.options(options()).ref(ref).editor(editor).popupRender(popup).motion());
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto input = snapshot(f, id).input;
    check(ref.focus(), "render focus");
    key(f, Key::down);
    check(f.services.next_frame_deadline().has_value() && popup_runs == 1, "finite motion did not start");
    (void)f.services.tick_animations(animation::AnimationTime::microseconds(1000000));
    sync(f);
    check(!static_cast<detail::WindowComponentParticipant&>(f.services.auto_complete()).next_auxiliary_deadline() &&
              popup_input.bound() && bounds(f, f.inputs.mounted_inputs().back().component).height > 0,
          "motion did not stop or popup content was not laid out");
    editor.set(AutoCompleteEditor{[&](InputProps props, auto prefix, auto suffix) {
        ++editor_runs;
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Text(u8"Custom editor label");
                 Input(std::move(props), std::move(prefix), std::move(suffix));
             }});
    }});
    sync(f);
    check(snapshot(f, id).input != input && editor_runs == 1 && popup_runs == 1, "editor replacement remounted popup");
    check(bounds(f, id).height > bounds(f, snapshot(f, id).input).height,
          "custom editor wrapper did not participate in measurement");
    const auto current = snapshot(f, id).input;
    popup.set(AutoCompletePopupRenderer{});
    sync(f);
    check(snapshot(f, id).input == current && !popup_input.bound(),
          "popup replacement remounted editor or retained old popup input");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    key(f, Key::escape);
    key(f, Key::down);
    check(!f.services.next_frame_deadline(), "reduced motion scheduled frames");
    clean(f);
}

void custom_editor_form() {
    Fixture f;
    FormRef form;
    FormField<String> field{String{u8"Initial"}};
    AutoCompleteRef ref;
    std::vector<AutoCompleteSource::Complete> requests;
    std::vector<std::shared_ptr<const std::atomic_bool>> cancelled;
    std::vector<std::function<void()>> posted;
    AutoCompleteSource source{[&](AsyncOptionsRequest request, auto complete) {
                                  cancelled.push_back(request.cancelled);
                                  requests.push_back(std::move(complete));
                                  return AsyncOptionsCancel{};
                              },
                              [&](auto task) { posted.push_back(std::move(task)); }};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"text", field).control(ref), FormItemContent{[&] {
                              AutoComplete(AutoCompleteProps{}
                                               .value(field.value())
                                               .onChange(field.onChange())
                                               .ref(ref)
                                               .source(source)
                                               .editor(AutoCompleteEditor{[](AutoCompleteEditorContext context) {
                                                   TextArea(context.text_area.rows(std::size_t{2}));
                                               }}));
                          }});
             }});
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    check(ref.focus(), "custom Form editor focus");
    edit(f, u8"request");
    const auto input = snapshot(f, id).input;
    auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "custom Form preedit");
    check(form.reset(), "custom Form reset");
    sync(f);
    check(snapshot(f, id).value == String{u8"Initial"} && snapshot(f, id).query == String{u8"Initial"} &&
              snapshot(f, id).input == input && !snapshot(f, id).pending && cancelled.back()->load() &&
              !f.inputs.editors().require(stamp.owner).composition().active,
          "custom Form reset retained transient state");
    requests.back()({{{u8"late", u8"late", u8"Late"}}, {}});
    check(posted.empty() && snapshot(f, id).rows.empty(), "reset accepted old custom editor request");
    stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"same-value"}, {}, stamp})), "custom same-value preedit");
    check(form.reset(), "custom same-value reset");
    sync(f);
    check(!f.inputs.editors().require(stamp.owner).composition().active, "custom same-value reset kept preedit");
    clean(f);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string test = argc > 1 ? argv[1] : "";
        if (test == "window") {
            window_and_groups();
        } else if (test == "async") {
            async_authority();
        } else if (test == "renderers") {
            renderers_and_motion();
        } else if (test == "form_editor") {
            custom_editor_form();
        } else {
            throw std::runtime_error("unknown test");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
