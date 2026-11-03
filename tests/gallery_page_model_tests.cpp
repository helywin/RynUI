#include "gallery_page_model.hpp"

#include <array>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_complete_catalog_and_page_identity() {
    using namespace rynui::example;
    GalleryPageModel model;
    require(model.pages().size() == 73 && ant_design_reference_version() == "6.6.5", "page source/version drifted");
    std::set<std::string_view> identities;
    for (const auto& page : model.pages()) {
        require(identities.insert(page.entry->identity).second && model.valid(page.route()),
                "canonical page is missing or duplicated");
        model.select(page.route());
        require(model.current().component_identity == page.entry->identity &&
                    model.find(page.entry->identity)->entry == page.entry &&
                    !gallery_page_status_label(page.entry->support_status).empty(),
                "page did not retain authoritative metadata");
    }
    require(model.counts().category == std::array<std::size_t, 7>{4, 7, 7, 18, 21, 11, 5},
            "category counts are not computed from the catalog");
    require(!model.find("ant.component.unknown") && !model.valid({GalleryPageKind::component, "ant.component.unknown"}),
            "unknown page silently became a registered component");
    const auto original = model.current();
    bool rejected{};
    try {
        model.select({GalleryPageKind::component, "missing"});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected && model.current() == original, "invalid navigation mutated current page");
    auto section = model.current();
    section.section = GalleryPageSection::support;
    require(model.valid(section), "registered page section rejected");
    section.section = static_cast<GalleryPageSection>(255);
    require(!model.valid(section), "unknown page section accepted");
}

void test_search_filters_preserve_page() {
    using namespace rynui::example;
    GalleryPageModel model;
    model.select(model.find("ant.component.button")->route());
    const auto original = model.current();
    const std::array aliases{
        std::pair{"Password", GalleryInputFamily::password}, std::pair{"密码", GalleryInputFamily::password},
        std::pair{" SEARCH ", GalleryInputFamily::search},   std::pair{"TextArea", GalleryInputFamily::text_area},
        std::pair{"多行", GalleryInputFamily::text_area},    std::pair{"OTP", GalleryInputFamily::otp},
    };
    for (const auto& [query, family] : aliases) {
        model.set_query(query);
        const auto results = model.results();
        require(results.size() == 1 && results.front().page->entry->identity == "ant.component.input" &&
                    results.front().family == family && model.current() == original && !model.current_matches(),
                "family search changed page or created another canonical entry");
    }
    model.set_query("按钮");
    require(model.current_matches() && !model.results().empty(), "Chinese component search failed");
    model.set_query("");
    model.set_filter(GallerySupportFilter::planned);
    require(!model.current_matches() && model.current() == original, "filter silently selected a different page");
    for (const auto& result : model.results()) {
        require(result.page->entry->support_status == GallerySupportStatus::planned && !result.page->native_scope(),
                "planned page was shown as native functionality");
    }
    model.set_query("not-an-existing-component");
    require(model.results().empty() && model.current() == original, "empty filter lost current state");
    model.set_filter(GallerySupportFilter::all);
    model.set_query("");
    require(model.results().size() == 73 && model.current_matches(), "clearing filters lost a component");
}

void test_all_statuses_and_invalid_catalogs() {
    using namespace rynui::example;
    std::array<AntDesignReferenceEntry, 6> entries{};
    constexpr std::array identities{"a", "b", "c", "d", "e", "f"};
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].identity = identities[i];
        entries[i].support_status = static_cast<GallerySupportStatus>(i);
    }
    GalleryPageModel model(entries);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        model.set_filter(static_cast<GallerySupportFilter>(i + 1));
        require(model.results().size() == 1 && model.results().front().page->entry == &entries[i] &&
                    model.counts().status[i] == 1 && model.pages()[i].native_scope() == (i < 2),
                "a zero-count production status has no defined page behavior");
    }
    entries[1].identity = entries[0].identity;
    bool rejected{};
    try {
        const GalleryPageModel invalid(entries);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "duplicate canonical identities accepted");
    require(!model.valid({GalleryPageKind::component, "a", GalleryInputFamily::otp}),
            "non-Input page accepted an Input family");
}

} // namespace

int main() {
    try {
        test_complete_catalog_and_page_identity();
        test_search_filters_preserve_page();
        test_all_statuses_and_invalid_catalogs();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
