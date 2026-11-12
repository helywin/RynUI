#include "gallery_demo_sample.hpp"

#include <stdexcept>
#include <utility>

namespace rynui::example {
namespace {
thread_local std::shared_ptr<GalleryDemoSampleContext> active;
} // namespace

void GalleryDemoSampleContext::callback(std::string_view target, std::string_view event, GalleryDemoJson payload) {
    callbacks.emplace_back(GalleryDemoJson::Object{{"seq", static_cast<std::int64_t>(callbacks.size())},
                                                   {"step", step},
                                                   {"time_us", time_us},
                                                   {"target", target},
                                                   {"key", ""},
                                                   {"event", event},
                                                   {"payload", std::move(payload)},
                                                   {"owner_generation", owner_generation},
                                                   {"layer", "native-core-dispatch"}});
}

bool GalleryDemoSampleContext::accept(std::string_view target) const {
    return parent_policy != "reject" || (fault == "input-reject-commit" && target == "gallery.input.controlled");
}

void GalleryDemoSampleContext::consume(std::string_view target, GalleryDemoJson props) {
    consumed_props.insert_or_assign(std::string{target}, std::move(props));
}

void GalleryDemoSampleContext::invoke(std::string_view target, std::string_view event, GalleryDemoJson payload,
                                      const std::function<void()>& original) {
    const auto invoke_once = [&] {
        if (original) {
            original();
        }
        if (event == "change") {
            proposed.insert_or_assign(std::string{target}, payload);
        }
        const bool opening = event == "open-change";
        const auto& getters = opening ? opens : values;
        const auto getter = getters.find(std::string{target});
        const bool controlled = (opening ? controlled_opens : controlled_values).contains(std::string{target});
        callback(target, event,
                 GalleryDemoJson::Object{{"proposed", payload},
                                         {"committed", getter == getters.end() ? payload : getter->second()},
                                         {"accepted", event == "activate" || !controlled || accept(target)}});
    };
    invoke_once();
    if (fault == "duplicate-callback") {
        // Duplicate the actual application callback, including its state update.
        // The observer records each real invocation; it does not edit a trace.
        invoke_once();
    }
}

GalleryDemoSampleScope::GalleryDemoSampleScope(std::shared_ptr<GalleryDemoSampleContext> context)
    : previous_(std::exchange(active, std::move(context))) {}

GalleryDemoSampleScope::~GalleryDemoSampleScope() {
    active = std::move(previous_);
}

std::shared_ptr<GalleryDemoSampleContext> gallery_demo_sample_context() {
    return active;
}

} // namespace rynui::example
