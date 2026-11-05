#include "component/form_model.hpp"
#include <iostream>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::detail;
using Time = animation::AnimationTime;

void check(bool v, const char* message) {
    if (!v) {
        throw std::runtime_error(message);
    }
}

struct Async final {
    std::vector<std::function<void()>> posts;
    std::vector<AsyncOptionsSource<FormIssue>::Complete> completions;
    std::vector<std::shared_ptr<const std::atomic_bool>> tickets;
    int cancels{};
    std::function<void()> on_cancel;

    AsyncOptionsSource<FormIssue> source() {
        return {[this](AsyncOptionsRequest request, auto complete) {
                    completions.push_back(std::move(complete));
                    tickets.push_back(request.cancelled);
                    return [this] {
                        ++cancels;
                        if (on_cancel) {
                            on_cancel();
                        }
                    };
                },
                [this](auto callback) { posts.push_back(std::move(callback)); }};
    }

    void drain() {
        auto current = std::exchange(posts, {});
        for (auto& callback : current) {
            callback();
        }
    }
};

void async_authority() {
    auto model = std::make_shared<FormModel>();
    auto field = make_form_field(String{u8"a"});
    Async source;
    configure_form_async(field, [&](auto, auto) { return source.source(); });
    model->add(String{u8"name"}, field);
    int finishes{};
    int stale{};
    model->finish = [&](FormResult r) {
        check(r.valid(), "invalid async success");
        ++finishes;
    };
    model->failed = [&](FormResult r) {
        check(r.out_of_date, "stale async submit not marked out of date");
        ++stale;
    };
    check(model->submit() && finishes == 0 && field->meta.get().validating,
          "submit blocked or failed to start async validation");
    check(!model->validate(), "sync validate falsely completed async rules");
    write_form_field(field, String{u8"b"});
    check(stale == 1 && !field->meta.get().validating && source.tickets[0]->load(),
          "write did not cancel pending submit");
    check(model->submit(), "new submit refused");
    source.completions[0]({{{String{u8"obsolete"}, false}}, {}});
    std::thread worker([&] { source.completions[1]({{}, {}}); });
    worker.join();
    check(finishes == 0 && field->meta.get().validating, "worker touched owner-thread signals");
    source.drain();
    check(finishes == 1 && !field->meta.get().validating && field->error.get().empty(), "current async result missing");
    check(model->submit(), "pending reset submit refused");
    check(model->reset() && !field->meta.get().validating && !field->meta.get().touched && stale == 2,
          "same-value reset left pending validation");
    source.completions.back()({{}, {}});
    source.drain();
    check(finishes == 1, "reset published obsolete success");
    check(model->submit(), "unmount submit refused");
    model->dispose();
    source.completions.back()({{}, {}});
    source.drain();
    check(finishes == 1 && !field->registered && !field->meta.get().validating, "unmount retained async state");
}

void debounce_dependencies_and_rules() {
    auto model = std::make_shared<FormModel>();
    auto name = make_form_field(String{u8"same"});
    auto confirmation = make_form_field(String{u8"same"});
    int calls{};
    FormRule rule;
    rule.validator = [&](const FormValue& value, const FormValidationContext& context) {
        ++calls;
        if (context.get(FormNamePath{u8"name"}) == value) {
            return std::vector<FormIssue>{};
        }
        return std::vector<FormIssue>{{String{u8"Does not match"}, false}};
    };
    configure_form_rules(confirmation, {rule}, false);
    configure_form_triggers(confirmation, FormValidateTrigger::Blur, 40, {FormNamePath{u8"name"}});
    model->add(String{u8"name"}, name);
    model->add(String{u8"confirmation"}, confirmation);
    write_form_field(name, String{u8"changed"});
    check(model->deadline() == Time::microseconds(40000), "dependency debounce deadline missing");
    check(model->tick(Time::microseconds(39999)) == 0 && calls == 0, "debounce ran early");
    model->tick(Time::microseconds(40000));
    check(calls == 1 && confirmation->meta.get().errors.size() == 1, "dependency validation did not read the snapshot");
    check(!model->deadline(), "idle validator retained a busy-loop deadline");
    const auto blur = form_blur_handler(confirmation);
    blur();
    model->remove(confirmation, confirmation->generation);
    model->add(String{u8"confirmation"}, confirmation);
    blur();
    check(!confirmation->due, "old blur callback scheduled a new registration");
    auto exact = make_form_field(std::optional<Decimal>{Decimal{"9007199254740993.1"}});
    FormRule bound;
    bound.type = FormRuleType::Number;
    bound.minimum = Decimal{"9007199254740993.2"};
    bound.message = String{u8"Too small"};
    configure_form_rules(exact, {bound}, false);
    model->add(FormNamePath{u8"exact"}, exact);
    auto result = model->validate();
    check(result && !result->valid() && exact->error.get() == String{u8"Too small"},
          "decimal rule converted exact value through double");
}

void explicit_submit_consumes_dependency_debounce() {
    auto model = std::make_shared<FormModel>();
    auto name = make_form_field(String{u8"old"});
    auto confirmation = make_form_field(String{u8"old"});
    Async source;
    configure_form_async(name, [&](auto, auto) { return source.source(); });
    configure_form_triggers(confirmation, FormValidateTrigger::Change, 150, {FormNamePath{u8"name"}});
    model->add(String{u8"name"}, name);
    model->add(String{u8"confirmation"}, confirmation);
    int finished{};
    int stale{};
    model->finish = [&](FormResult result) {
        check(result.valid() && std::get<String>(result.values[0].value) == String{u8"latest"},
              "explicit submit lost its typed snapshot");
        ++finished;
    };
    model->failed = [&](FormResult) {
        ++stale;
    };
    write_form_field(name, String{u8"latest"});
    write_form_field(confirmation, String{u8"latest"});
    check(confirmation->due.has_value(), "dependency debounce setup missing");
    check(model->submit() && name->meta.get().validating, "explicit async submit did not start");
    check(!confirmation->due, "explicit submit retained the synchronous dependency debounce");
    model->tick(Time::microseconds(150000));
    check(name->meta.get().validating && !source.tickets.front()->load() && stale == 0,
          "old dependency timer cancelled a newer explicit submission");
    source.completions.front()({{}, {}});
    source.drain();
    check(finished == 1 && stale == 0 && !name->meta.get().validating,
          "explicit submission failed to publish after the old debounce deadline");
    model->dispose();
}

void throws_and_boundaries() {
    auto model = std::make_shared<FormModel>();
    auto field = make_form_field(false);
    Async source;
    configure_form_async(field, [&](auto, auto) { return source.source(); });
    model->add(String{u8"field"}, field);
    bool eligible = true;
    model->eligible = [&] {
        return eligible;
    };
    check(model->validate_async({}), "async validation refused");
    eligible = false;
    model->boundary_changed();
    check(!field->meta.get().validating && source.tickets.back()->load(), "hidden form retained pending validation");
    eligible = true;
    field->async = [](auto, auto) -> AsyncOptionsSource<FormIssue> {
        throw std::runtime_error("provider");
    };
    bool threw{};
    try {
        model->validate_async({});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !model->busy && !field->meta.get().validating, "provider throw stranded validation");
    field->async = [&](auto, auto) {
        return source.source();
    };
    check(model->validate_async([&](FormResult) { model->dispose(); }), "self-destroy validator refused");
    source.completions.back()({{}, {}});
    source.drain();
    check(!model->alive && !model->busy && !field->registered, "self-destroy completion retained model");
}

void reentrant_compound_cancellation_and_throws() {
    auto model = std::make_shared<FormModel>();
    auto first = make_form_field(String{u8"first"});
    auto second = make_form_field(String{u8"second"});
    Async source;
    configure_form_async(first, [&](auto, auto) { return source.source(); });
    configure_form_async(second, [&](auto, auto) { return source.source(); });
    model->add(String{u8"first"}, first);
    model->add(String{u8"second"}, second);
    check(model->validate_async({}), "compound validation refused");
    bool restarted{};
    source.on_cancel = [&] {
        if (std::exchange(restarted, true)) {
            return;
        }
        check(source.tickets[0]->load() && source.tickets[1]->load(),
              "application cancellation ran before every old ticket was invalidated");
        FormValidateOptions options;
        options.names = {FormNamePath{u8"first"}};
        check(model->validate_async({}, options), "reentrant replacement validation refused");
    };
    check(model->cancel_validation() && first->meta.get().validating && !second->meta.get().validating,
          "old cancellation overwrote replacement state");
    source.completions[0]({{{String{u8"obsolete"}, false}}, {}});
    source.completions.back()({{}, {}});
    source.drain();
    check(!first->meta.get().validating && first->meta.get().errors.empty(), "replacement validation did not complete");
    source.on_cancel = {};
    model->dispose();
    Signal<String> authority{String{u8"authority"}};
    auto controlled = make_form_field(String{}, bind([authority] { return FormValue{authority.get()}; }),
                                      [](FormValue) { throw std::runtime_error("change"); });
    auto owner = std::make_shared<FormModel>();
    configure_form_async(controlled, [&](auto, auto) { return source.source(); });
    owner->add(String{u8"controlled"}, controlled);
    int cancelled{};
    check(owner->validate_async([&](FormResult result) {
        check(result.out_of_date, "throw did not report stale result");
        ++cancelled;
    }),
          "controlled pending validation refused");
    bool threw{};
    try {
        write_form_field(controlled, String{u8"requested"});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && cancelled == 1 && !controlled->meta.get().validating && authority.get() == String{u8"authority"},
          "throwing controlled callback stranded pending validation or forged authority");
}

void dirty_validate_first_and_metadata_exceptions() {
    auto model = std::make_shared<FormModel>();
    auto field = make_form_field(String{u8"value"});
    int validations{};
    field->validator = [&](const FormValue&) {
        ++validations;
        return String{};
    };
    model->add(String{u8"field"}, field);
    check(model->validate()->valid() && validations == 1, "initial validation missing");
    FormValidateOptions options;
    options.dirty = true;
    options.validate_only = true;
    check(model->validate_async({}, options) && validations == 2,
          "dirty validation omitted a previously validated untouched field");
    auto invalid = make_form_field(String{});
    configure_form_required(invalid, String{u8"required"});
    configure_form_rules(invalid, {}, true);
    Async source;
    configure_form_async(invalid, [&](auto, auto) { return source.source(); });
    auto other = std::make_shared<FormModel>();
    other->add(String{u8"invalid"}, invalid);
    check(other->validate_async({}) && source.completions.empty() && !invalid->meta.get().validating,
          "validate-first started async work after synchronous failure");
    invalid->validate_first = false;
    check(other->validate_async({}), "metadata cancellation setup refused");
    Scope scope;
    auto observer = effect(scope, [&] {
        const auto meta = invalid->meta.get();
        if (!meta.validating && meta.out_of_date) {
            throw std::runtime_error("metadata observer");
        }
    });
    bool threw{};
    try {
        other->cancel_validation();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && source.tickets.back()->load() && !invalid->meta.get().validating && !other->busy,
          "metadata throw leaked pending state or cancellation authority");
    scope.dispose();
}

void registration_reentrancy_authority() {
    auto first = std::make_shared<FormModel>();
    auto second = std::make_shared<FormModel>();
    auto field = make_form_field(String{u8"original"});
    const auto generation = first->add(String{u8"first"}, field);
    const auto old = form_change_handler(field);
    Scope scope;
    bool moved{};
    auto observer = effect(scope, [&] {
        if (field->meta.get().touched && !moved) {
            moved = true;
            first->remove(field, generation);
            second->add(String{u8"second"}, field);
        }
    });
    old(String{u8"stale"});
    check(moved && read_form_field(field) == FormValue{String{u8"original"}} && field->owner.lock() == second,
          "touched observer rebound the field but an old callback wrote its new owner");
    scope.dispose();
    FormRegistrationScope old_mount{field, generation};
    const auto late_old = form_change_handler(field);
    late_old(String{u8"stale late"});
    check(read_form_field(field) == FormValue{String{u8"original"}},
          "callback created late in an old item captured a new registration");

    auto source_field = make_form_field(String{u8"start"});
    Async source;
    source_field->async = [&](auto, auto) {
        return AsyncOptionsSource<FormIssue>{[&](AsyncOptionsRequest request, auto complete) {
                                                 source.tickets.push_back(request.cancelled);
                                                 source.completions.push_back(std::move(complete));
                                                 return [&] {
                                                     second->add(String{u8"replacement"}, source_field);
                                                 };
                                             },
                                             [&](auto post) { source.posts.push_back(std::move(post)); }};
    };
    const auto ticket = first->add(String{u8"async"}, source_field);
    check(first->validate_async({}), "remove reentry setup failed");
    first->remove(source_field, ticket);
    check(source_field->registered && source_field->owner.lock() == second &&
              second->get(FormNamePath{u8"replacement"}).has_value(),
          "remove invalidated a replacement registration from its cancellation callback");

    auto controlled = make_form_field(String{}, bind([] { return FormValue{String{u8"accepted"}}; }),
                                      [&](FormValue) { second->remove(field, field->generation); });
    second->remove(field, field->generation);
    auto bulk = std::make_shared<FormModel>();
    bulk->add(String{u8"controlled"}, controlled);
    bulk->add(String{u8"later"}, field);
    controlled->change = [&](FormValue) {
        bulk->remove(field, field->generation);
        second->add(String{u8"new"}, field);
    };
    FormData values;
    values.set(FormNamePath{u8"controlled"}, FormData{String{u8"request"}});
    values.set(FormNamePath{u8"later"}, FormData{String{u8"must not write"}});
    bool threw{};
    try {
        bulk->set_values(values);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !bulk->alive && field->owner.lock() == second &&
              read_form_field(field) == FormValue{String{u8"original"}},
          "bulk update wrote a replacement registration or left a partial live model");
    first->dispose();
    second->dispose();
}

void self_destroying_factory_and_reentrant_getters() {
    auto model = std::make_shared<FormModel>();
    auto field = make_form_field(String{});
    Async source;
    field->async = [&](auto, auto) {
        model->dispose();
        return source.source();
    };
    model->add(String{u8"field"}, field);
    check(!model->validate_async({}) && source.completions.empty() && !field->meta.get().validating &&
              !field->registered && !model->deadline(),
          "self-destroying async factory started transport after retirement");

    auto owner = std::make_shared<FormModel>();
    auto other = make_form_field(String{u8"other"});
    bool armed{};
    auto controlled = make_form_field(String{}, bind([&] {
                                          if (std::exchange(armed, false)) {
                                              owner->remove(other, other->generation);
                                          }
                                          return FormValue{String{u8"snapshot"}};
                                      }));
    owner->add(String{u8"controlled"}, controlled);
    owner->add(String{u8"other"}, other);
    armed = true;
    const auto result = owner->validate();
    check(result && result->out_of_date && !other->registered && !owner->busy,
          "reentrant controlled getter published success for changed topology");
    owner->dispose();
}

} // namespace

int main() {
    try {
        async_authority();
        debounce_dependencies_and_rules();
        explicit_submit_consumes_dependency_debounce();
        throws_and_boundaries();
        reentrant_compound_cancellation_and_throws();
        dirty_validate_first_and_metadata_exceptions();
        registration_reentrancy_authority();
        self_destroying_factory_and_reentrant_getters();
        std::cout << "Form async contracts passed (8 cases)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
