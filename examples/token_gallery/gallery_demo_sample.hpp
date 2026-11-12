#pragma once

#include "gallery_demo_protocol.hpp"

#include <functional>
#include <memory>
#include <string>
#include <set>

namespace rynui::example {

struct GalleryDemoSampleContext final {
    GalleryDemoJson fixture;
    std::function<void(std::string_view, bool)> factory_boundary;
    GalleryDemoJson::Array callbacks;
    GalleryDemoJson::Object consumed_props;
    std::string step{"setup"};
    std::string parent_policy{"accept"};
    std::string fault{"none"};
    std::map<std::string, std::function<GalleryDemoJson()>> values;
    std::map<std::string, std::function<GalleryDemoJson()>> opens;
    std::set<std::string> controlled_values;
    std::set<std::string> controlled_opens;
    GalleryDemoJson::Object proposed;
    std::int64_t time_us{};
    std::int64_t owner_generation{};

    void callback(std::string_view target, std::string_view event, GalleryDemoJson payload);
    [[nodiscard]] bool accept(std::string_view target) const;
    void consume(std::string_view target, GalleryDemoJson props);
    void invoke(std::string_view target, std::string_view event, GalleryDemoJson payload,
                const std::function<void()>& original);
};

class GalleryDemoSampleScope final {
public:
    explicit GalleryDemoSampleScope(std::shared_ptr<GalleryDemoSampleContext> context);
    ~GalleryDemoSampleScope();
    GalleryDemoSampleScope(const GalleryDemoSampleScope&) = delete;
    GalleryDemoSampleScope& operator=(const GalleryDemoSampleScope&) = delete;

private:
    std::shared_ptr<GalleryDemoSampleContext> previous_;
};

[[nodiscard]] std::shared_ptr<GalleryDemoSampleContext> gallery_demo_sample_context();

} // namespace rynui::example
