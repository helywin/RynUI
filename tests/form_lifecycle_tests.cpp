#include "component/form_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <map>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* message) {
    if (!v) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({600, 900}, {0, 0, 600, 900}), "Form sync failed");
}

void numeric_drafts_and_removed_tickets() {
    Fixture f;
    FormRef form;
    FormListRef list;

    struct Row final {
        FormField<InputNumberValue> amount{1.0};
        InputRef ref;
        std::function<void(InputNumberValue)> change;
    };

    std::map<std::uint64_t, Row> rows;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormList(FormListProps{}.name(FormNamePath{u8"rows"}).ref(list).initialValue({{}, {}}),
                          [&](FormListField row) {
                              auto& state = rows[row.key()];
                              FormItem(FormItemProps{}.field(FormNamePath{u8"amount"}, state.amount).control(state.ref),
                                       FormItemContent{[&state] {
                                           state.change = state.amount.onChange();
                                           InputNumber(InputNumberProps{}
                                                           .value(state.amount.value())
                                                           .onChange(state.change)
                                                           .ref(state.ref));
                                       }});
                          });
             }});
    }});
    sync(f);
    const auto row = list.fields()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    auto& state = rows[row.key()];
    check(state.ref.focus({InputFocusCursor::All}), "list numeric focus failed");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(input::TextCommitted{String{u8"1."}, stamp})), "list numeric draft failed");
    check(bool(f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {0, 0}, stamp})), "list preedit failed");
    check(list.move(0, 1), "numeric row move failed");
    sync(f);
    check(f.inputs.editors().require(input.editor).value() == "1." &&
              f.inputs.editors().require(input.editor).composition().active && f.inputs.sessions().active() == stamp &&
              state.amount.get() == 1.0,
          "move lost rich numeric draft/preedit/focus");
    const auto callback = state.change;
    check(list.remove({1}), "numeric row remove failed");
    sync(f);
    callback(99.0);
    check(state.amount.get() == 1.0 && !bool(f.inputs.dispatch(input::TextCommitted{String{u8"99"}, stamp})),
          "removed list editor or callback ticket wrote another row");
}

void list_values_rules_and_failure_retirement() {
    Fixture f;
    FormRef form;
    FormListRef list;
    FormField<String> unrelated{String{u8"healthy"}};
    bool throw_renderer{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"unrelated"}, unrelated),
                          FormItemContent{[] { Input(InputProps{}); }});
                 FormList(FormListProps{}
                              .name(FormNamePath{u8"rows"})
                              .ref(list)
                              .rules({[](const FormData::Array& rows, const FormValidationContext&) {
                                  if (rows.empty()) {
                                      return std::vector<FormIssue>{{String{u8"Need a row"}, false}};
                                  }
                                  return std::vector<FormIssue>{};
                              }}),
                          [&](FormListField) {
                              if (throw_renderer) {
                                  throw std::runtime_error("renderer");
                              }
                              FormField<String> name;
                              FormItem(FormItemProps{}.field(FormNamePath{u8"name"}, name),
                                       FormItemContent{[] { Input(InputProps{}); }});
                          });
             }});
    }});
    sync(f);
    const auto empty = form.getFieldsValue();
    check(empty.at(FormNamePath{u8"rows"}) &&
              std::get<FormData::Array>(empty.at(FormNamePath{u8"rows"})->value).empty(),
          "empty list disappeared from values");
    check(form.validate()->errors.size() == 1 && list.meta().errors.size() == 1, "list custom rule/meta missing");
    FormData update;
    FormData row;
    row.set(FormNamePath{u8"name"}, FormData{FormValue{String{u8"new"}}});
    update.set(FormNamePath{u8"rows"}, FormData{FormData::Array{row, row}});
    check(form.setFieldsValue(update) && list.fields().size() == 2 &&
              form.getFieldValue(FormNamePath{u8"rows", 1, u8"name"}) == FormValue{String{u8"new"}},
          "structured list update did not add typed rows");
    const auto key = list.fields()[0].key();
    row.set(FormNamePath{u8"name"}, FormData{FormValue{String{u8"edited"}}});
    update.set(FormNamePath{u8"rows"}, FormData{FormData::Array{row}});
    check(form.setFieldsValue(update) && list.fields()[0].key() == key && form.validate()->valid(),
          "list update remounted or failed validation");
    check(form.resetFields({FormNamePath{u8"rows"}}) && list.fields().empty() && unrelated.get() == String{u8"healthy"},
          "scoped list reset changed unrelated field");
    throw_renderer = true;
    bool threw{};
    try {
        static_cast<void>(list.add());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !list.bound() && form.bound() &&
              form.getFieldValue(FormNamePath{u8"unrelated"}) == FormValue{String{u8"healthy"}},
          "renderer throw did not retire only the complete list");
    unrelated.set(String{u8"still healthy"});
    sync(f);
    check(form.validate()->valid(), "retired list poisoned unrelated Form components");
}

void initial_values_and_controlled_refusal() {
    Fixture f;
    FormRef form;
    Signal<String> source{String{u8"authority"}};
    FormField<String> field{String{u8"constructor"}};
    FormField<String> controlled{String{u8"reset"}, source, [](String) {}};
    FormData initial;
    initial.set(FormNamePath{u8"name"}, FormData{FormValue{String{u8"form initial"}}});
    InputRef ref;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).initialValues(initial), FormContent{[&] {
                 FormItem(
                     FormItemProps{}.field(String{u8"name"}, field).initialValue(FormValue{String{u8"item initial"}}),
                     FormItemContent{[] { Input(InputProps{}); }});
                 FormItem(FormItemProps{}.field(String{u8"controlled"}, controlled).control(ref), FormItemContent{[&] {
                              Input(InputProps{}.value(controlled.value()).onChange(controlled.onChange()).ref(ref));
                          }});
             }});
    }});
    sync(f);
    check(field.get() == String{u8"form initial"}, "Form initialValues lost priority");
    field.set(String{u8"edit"});
    check(ref.focus({InputFocusCursor::All}), "controlled input focus failed");
    static_cast<void>(f.inputs.dispatch(input::TextCommitted{String{u8"refused"}, f.inputs.sessions().active()}));
    check(controlled.get() == source.get(), "controlled refusal forged field authority");
    check(form.reset() && field.get() == String{u8"form initial"} && controlled.get() == String{u8"authority"},
          "initial/reset authority mismatch");
}

void async_list_and_custom_adapter() {
    Fixture f;
    FormRef form;
    FormListRef list;
    FormValidationComplete complete;
    std::vector<std::function<void()>> posts;
    std::shared_ptr<const std::atomic_bool> cancelled;
    int finished{};
    int stale{};
    FormField<String> field{String{u8"initial"}};
    InputRef ref;
    int mounts{};
    f.services.mount(Content{[&] {
        Form(FormProps{}
                 .ref(form)
                 .onFinish([&](FormResult result) {
                     check(result.valid() && result.warnings.size() == 1, "list async warnings result missing");
                     ++finished;
                 })
                 .onFinishFailed([&](FormResult result) {
                     check(result.out_of_date, "list old submit not marked stale");
                     ++stale;
                 }),
             FormContent{[&] {
                 FormList(FormListProps{}
                              .name(FormNamePath{u8"rows"})
                              .ref(list)
                              .asyncValidator(
                                  [&](FormData::Array, FormValidationContext context, FormValidationComplete callback) {
                                      complete = std::move(callback);
                                      cancelled = context.cancelled;
                                      return AsyncOptionsCancel{};
                                  },
                                  [&](auto callback) { posts.push_back(std::move(callback)); }),
                          [](FormListField) { Text(u8"row"); });
                 FormItem(FormItemProps{}.validateStatus(FormValidateStatus::Error), FormItemContent{[&] {
                              FormItem(FormItemProps{}.field(String{u8"custom"}, field).noStyle(), FormItemContent{[&] {
                                           FormControl(
                                               FormControlAdapter<String>{[&](FormControlBinding<String> binding) {
                                                   ++mounts;
                                                   Input(InputProps{}
                                                             .value(binding.value())
                                                             .onChange(binding.onChange())
                                                             .disabled(binding.disabled())
                                                             .readOnly(binding.readOnly())
                                                             .status(binding.status())
                                                             .ref(ref));
                                                   binding.control(ref);
                                               }});
                                       }});
                          }});
             }});
    }});
    sync(f);
    const auto input = f.inputs.mounted_inputs().front();
    check(f.inputs.status(input.component) == InputStatus::Error, "noStyle did not inherit parent validation status");
    check(form.submitAsync() && list.meta().validating && !form.validate(),
          "list async validation did not become pending");
    check(list.add() && cancelled->load() && !list.meta().validating && stale == 1,
          "list mutation retained old async submit");
    complete({});
    for (auto& callback : posts) {
        callback();
    }
    posts.clear();
    check(finished == 0, "list mutation published obsolete success");
    check(form.submitAsync(), "current list submit refused");
    complete({{String{u8"Advisory"}, true}});
    for (auto& callback : posts) {
        callback();
    }
    posts.clear();
    check(finished == 1 && list.meta().warnings.size() == 1 && !list.meta().validating,
          "list owner-thread completion missing");
    check(ref.focus({InputFocusCursor::End}), "custom adapter ref focus failed");
    const auto stamp = f.inputs.sessions().active();
    static_cast<void>(f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {0, 0}, stamp}));
    check(form.reset() && !f.inputs.editors().require(input.editor).composition().active && mounts == 1,
          "custom adapter same-value reset remounted or retained preedit");
}

void nested_provider_isolation() {
    Fixture f;
    FormRef outer;
    FormRef inner;
    int outer_events{};
    int inner_events{};
    f.services.mount(Content{[&] {
        FormProvider(FormProviderProps{}.onFormFinish([&](FormProviderEvent event) {
            check(event.forms.size() == 1 && event.name == String{u8"same"}, "outer provider leaked nested registry");
            ++outer_events;
        }),
                     FormProviderContent{[&] {
                         Form(FormProps{}.ref(outer).name(String{u8"same"}), FormContent{[&] {
                                  FormProvider(FormProviderProps{}.onFormFinish([&](FormProviderEvent event) {
                                      check(event.forms.size() == 1 && event.name == String{u8"same"},
                                            "nested provider leaked outer registry");
                                      ++inner_events;
                                  }),
                                               FormProviderContent{[&] {
                                                   Form(FormProps{}.ref(inner).name(String{u8"same"}),
                                                        FormContent{[] { Text(u8"nested"); }});
                                               }});
                              }});
                     }});
    }});
    sync(f);
    check(inner.submit() && inner_events == 1 && outer_events == 0, "nested provider finish crossed scope");
    check(outer.submit() && outer_events == 1 && inner_events == 1, "outer provider finish crossed scope");
}

void preservation_and_live_ref_watchers() {
    Fixture f;
    FormRef form;
    FormField<String> kept{String{u8"initial"}};
    FormField<String> discarded{String{u8"discard initial"}};
    std::optional<FormValue> observed;
    int watches{};
    int meta_watches{};
    Scope observers;
    const auto watched = useWatch(FormNamePath{u8"kept"}, form);
    auto values = effect(observers, [&] {
        observed = watched.get();
        ++watches;
    });
    auto meta = effect(observers, [&] {
        static_cast<void>(form.isFieldTouched(FormNamePath{u8"kept"}));
        ++meta_watches;
    });
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Form(FormProps{}.ref(form).clearOnDestroy(), FormContent{[&] {
                          FormItem(FormItemProps{}.field(String{u8"kept"}, kept),
                                   FormItemContent{[] { Input(InputProps{}); }});
                          FormItem(FormItemProps{}.field(String{u8"discard"}, discarded).preserve(false),
                                   FormItemContent{
                                       [] { Input(InputProps{}.defaultValue(String{u8"ignored control default"})); }});
                      }});
             }});
    }});
    sync(f);
    check(observed == FormValue{String{u8"initial"}}, "watcher created before mount did not bind");
    const auto before = watches;
    const auto before_meta = meta_watches;
    discarded.set(String{u8"changed"});
    check(watches == before && meta_watches == before_meta, "unrelated field reran fine-grained value/meta watchers");
    const auto discard_item = f.services.form().items()[1];
    check(f.services.destroy(discard_item) && discarded.get() == String{u8"discard initial"} &&
              !form.getFieldsValue(true).at(FormNamePath{u8"discard"}),
          "preserve=false retained removed state");
    kept.set(String{u8"edited"});
    check(observed == FormValue{String{u8"edited"}}, "watcher did not observe field authority");
    const auto form_id = f.services.form().mounted().front();
    check(f.services.destroy(form_id) && !observed && kept.get() == String{u8"initial"},
          "clearOnDestroy did not clear values and watcher");
    const auto holder = f.services.components().root_components().front();
    f.services.append_slot(holder, Content{[&] {
                               Form(FormProps{}.ref(form), FormContent{[&] {
                                        FormItem(FormItemProps{}.field(String{u8"kept"}, kept),
                                                 FormItemContent{[] { Input(InputProps{}); }});
                                    }});
                           }});
    sync(f);
    check(observed == FormValue{String{u8"initial"}}, "watcher failed to follow a rebound FormRef");
    observers.dispose();
}

void rich_slots_and_native_feedback_geometry() {
    Fixture f;
    FormRef form;
    FormField<String> field;
    field.required();
    InputRef extra;
    int labels{};
    int helps{};
    int extras{};
    int icons{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}
                              .field(String{u8"name"}, field)
                              .hasFeedback()
                              .labelContent(FormItemLabel{[&] {
                                  ++labels;
                                  Text(u8"Rich label");
                              }})
                              .helpContent(FormItemHelp{[&] {
                                  ++helps;
                                  const auto status = useFormStatus();
                                  Text(TextProps{}.content(bind([status] {
                                      return status.get().errors.empty() ? String{u8"Help"}
                                                                         : String{u8"Hidden while invalid"};
                                  })));
                              }})
                              .extraContent(FormItemExtra{[&] {
                                  ++extras;
                                  Input(InputProps{}.defaultValue(String{u8"extra"}).ref(extra));
                              }})
                              .feedbackIcon(FormItemFeedbackIcon{[&] {
                                  ++icons;
                                  Icon(IconProps{}.name(IconName::CheckCircleFilled));
                              }}),
                          FormItemContent{[] { Input(InputProps{}); }});
             }});
    }});
    sync(f);
    const auto item = f.services.form().items().front();
    const auto parts = f.services.form().snapshot(item);
    const auto control = f.inputs.mounted_inputs()[0];
    const auto unrelated = f.inputs.mounted_inputs()[1];
    check(extra.focus({InputFocusCursor::All}), "rich extra focus failed");
    static_cast<void>(
        f.inputs.dispatch(input::TextCommitted{String{u8"retained draft"}, f.inputs.sessions().active()}));
    check(field.get().empty(), "an unrelated rich extra editor was injected with the field value");
    check(form.validate()->errors.size() == 1, "rich field rule missing");
    sync(f);
    const auto icon = f.nodes.require(f.services.components().root(parts.icon)).bounds;
    const auto bounds = f.nodes.require(f.services.components().root(parts.control)).bounds;
    check(icon.width > 0 && icon.x >= bounds.x + bounds.width, "hasFeedback did not reserve actual suffix geometry");
    field.set(String{u8"valid"});
    check(form.validate()->valid(), "rich field did not recover");
    sync(f);
    check(labels == 1 && helps == 1 && extras == 1 && icons == 1 &&
              f.services.components().contains(control.component) &&
              f.inputs.editors().require(unrelated.editor).value() == "retained draft",
          "validation remounted rich slots or reset unrelated drafts");
    check(form.reset() && f.inputs.editors().require(unrelated.editor).value() == "retained draft",
          "reset wrote an unrelated descendant");
    f.services.dispose();
}

void throwing_controlled_set_retires_scope() {
    Fixture f;
    FormRef form;
    InputRef outside;
    Signal<String> source{String{u8"accepted"}};
    FormField<String> controlled{String{u8"accepted"}, bind([&] { return source.get(); }),
                                 [](String) { throw std::runtime_error("application setter"); }};
    FormField<String> ordinary{String{u8"before"}};
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(outside));
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"first"}, ordinary),
                          FormItemContent{[] { Input(InputProps{}); }});
                 FormItem(FormItemProps{}.field(String{u8"controlled"}, controlled),
                          FormItemContent{[] { Input(InputProps{}); }});
             }});
    }});
    sync(f);
    FormData values;
    values.set(FormNamePath{u8"first"}, FormData{String{u8"after"}});
    values.set(FormNamePath{u8"controlled"}, FormData{String{u8"requested"}});
    bool threw{};
    try {
        static_cast<void>(form.setFieldsValue(values));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    sync(f);
    check(threw && !form.bound() && f.inputs.mounted_inputs().size() == 1 && outside.bound(),
          "throwing setter left a partially applied Form UI alive");
    check(outside.focus(), "unrelated control lost focus after Form retirement");
    static_cast<void>(f.inputs.dispatch(input::TextCommitted{String{u8"healthy"}, f.inputs.sessions().active()}));
    check(f.inputs.editors().require(f.inputs.mounted_inputs().front().editor).value() == "healthy",
          "Form retirement poisoned unrelated input");
}

void unmounted_store_and_later_registration() {
    Fixture f;
    FormRef form;
    FormData initial;
    initial.set(FormNamePath{u8"reserved"}, FormData{String{u8"initial reserved"}});
    f.services.mount(Content{[&] { Form(FormProps{}.ref(form).initialValues(initial), FormContent{[] {}}); }});
    sync(f);
    check(form.getFieldValue(FormNamePath{u8"reserved"}) == FormValue{String{u8"initial reserved"}} &&
              !form.getFieldsValue().at(FormNamePath{u8"reserved"}) &&
              form.getFieldsValue(true).at(FormNamePath{u8"reserved"}),
          "unmounted initial store did not respect getFieldsValue preservation");
    const auto watch = form.watch(FormNamePath{u8"profile", u8"name"});
    int changes{};
    Scope scope;
    auto observer = effect(scope, [&] {
        static_cast<void>(watch.get());
        ++changes;
    });
    check(form.setFieldValue(FormNamePath{u8"profile", u8"name"}, String{u8"stored"}) && changes == 2 &&
              watch.get() == FormValue{String{u8"stored"}},
          "unmounted typed field update or watch failed");
    FormField<String> name{String{u8"constructor"}};
    name.required();
    f.services.append_slot(f.services.form().mounted().front(), Content{[&] {
                               FormItem(FormItemProps{}.field(FormNamePath{u8"profile", u8"name"}, name),
                                        FormItemContent{[] { Input(InputProps{}); }});
                           }});
    sync(f);
    check(name.get() == String{u8"stored"}, "later field registration ignored programmatic stored value");
    name.set({});
    check(!form.validate()->valid() && !form.getFieldsError().front().meta.errors.empty(), "store rule setup failed");
    check(form.setFieldValue(FormNamePath{u8"profile", u8"name"}, String{}) &&
              form.getFieldsError().front().meta.errors.empty(),
          "same-value setter did not reset validation messages");
    check(form.resetFields({FormNamePath{u8"reserved"}}) &&
              form.getFieldValue(FormNamePath{u8"reserved"}) == FormValue{String{u8"initial reserved"}},
          "unmounted partial reset ignored initial values");
    check(form.reset() && name.get() == String{u8"constructor"} && !form.getFieldValue(FormNamePath{u8"profile"}),
          "full reset did not clear noninitial unmounted store or restore field defaults");
    scope.dispose();
}

void unmounted_list_drafts_and_pending_removal() {
    Fixture f;
    FormRef form;
    FormListRef list;
    FormData first;
    first.set(FormNamePath{u8"note"}, FormData{String{u8"first draft"}});
    FormData second;
    second.set(FormNamePath{u8"note"}, FormData{String{u8"second draft"}});
    FormValidationComplete complete;
    std::vector<std::function<void()>> posts;
    std::shared_ptr<const std::atomic_bool> ticket;
    int stale{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinishFailed([&](FormResult value) {
            check(value.out_of_date, "removed list emitted nonstale submit");
            ++stale;
        }),
             FormContent{[&] {
                 FormList(FormListProps{}
                              .name(FormNamePath{u8"rows"})
                              .ref(list)
                              .initialValue({first, second})
                              .asyncValidator(
                                  [&](FormData::Array, FormValidationContext context, auto callback) {
                                      ticket = context.cancelled;
                                      complete = std::move(callback);
                                      return AsyncOptionsCancel{};
                                  },
                                  [&](auto callback) { posts.push_back(std::move(callback)); }),
                          [](FormListField) { Text(u8"Retained row without a mounted editor"); });
             }});
    }});
    sync(f);
    check(list.move(0, 1) &&
              form.getFieldValue(FormNamePath{u8"rows", 1, u8"note"}) == FormValue{String{u8"first draft"}},
          "list move lost unmounted child store values");
    check(form.reset() && form.getFieldValue(FormNamePath{u8"rows", 0, u8"note"}) == FormValue{String{u8"first draft"}},
          "list reset lost unmounted initial child values");
    check(form.submitAsync() && list.meta().validating, "pending list removal setup failed");
    const auto form_id = f.services.form().mounted().front();
    // The list is the direct retained child of this form, with its own resource scope.
    const auto list_id = f.services.components().children(form_id).front();
    check(f.services.destroy(list_id), "pending list destroy failed");
    check(ticket->load() && stale == 1 && !list.bound() && form.validate()->valid(),
          "destroying an async-only list left Form pending or transport authoritative");
    complete({});
    for (auto& post : posts) {
        post();
    }
    check(stale == 1, "removed list posted another result");
}

} // namespace

int main() {
    try {
        numeric_drafts_and_removed_tickets();
        list_values_rules_and_failure_retirement();
        initial_values_and_controlled_refusal();
        async_list_and_custom_adapter();
        nested_provider_isolation();
        preservation_and_live_ref_watchers();
        rich_slots_and_native_feedback_geometry();
        throwing_controlled_set_retires_scope();
        unmounted_store_and_later_registration();
        unmounted_list_drafts_and_pending_removal();
        std::cout << "Form lifecycle contracts passed (10 cases)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
