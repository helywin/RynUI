#include "gallery_page_model.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace rynui::example {
namespace {

std::string folded(std::string_view text) {
    std::string result(text);
    for (auto& character : result) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    const auto first = result.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    return result.substr(first, result.find_last_not_of(" \t\r\n") - first + 1);
}

struct FamilyAlias final {
    std::string_view english;
    std::string_view chinese;
    GalleryInputFamily family;
};

constexpr std::array aliases{
    FamilyAlias{"input", "单行", GalleryInputFamily::input},
    FamilyAlias{"search", "搜索", GalleryInputFamily::search},
    FamilyAlias{"password", "密码", GalleryInputFamily::password},
    FamilyAlias{"textarea", "多行", GalleryInputFamily::text_area},
    FamilyAlias{"otp", "验证码", GalleryInputFamily::otp},
};

bool valid_family(GalleryInputFamily value) noexcept {
    return std::ranges::any_of(aliases, [value](const auto& alias) { return alias.family == value; });
}

} // namespace

bool GalleryPageDescriptor::native_scope() const noexcept {
    return entry->support_status == GallerySupportStatus::implemented ||
           entry->support_status == GallerySupportStatus::partial;
}

GalleryPageRoute GalleryPageDescriptor::route(GalleryInputFamily family) const {
    return {GalleryPageKind::component, std::string(entry->identity), family, {}};
}

GalleryPageModel::GalleryPageModel(std::span<const AntDesignReferenceEntry> entries) {
    pages_.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto status = static_cast<std::size_t>(entry.support_status);
        const auto category = static_cast<std::size_t>(entry.category);
        if (entry.identity.empty() || find(entry.identity) || status >= counts_.status.size() ||
            category >= counts_.category.size()) {
            throw std::invalid_argument("Gallery page catalog contains an invalid or duplicate entry");
        }
        pages_.push_back({&entry});
        ++counts_.status[status];
        ++counts_.category[category];
    }
}

std::span<const GalleryPageDescriptor> GalleryPageModel::pages() const noexcept {
    return pages_;
}

const GalleryPageDescriptor* GalleryPageModel::find(std::string_view identity) const noexcept {
    const auto found =
        std::ranges::find_if(pages_, [identity](const auto& page) { return page.entry->identity == identity; });
    return found == pages_.end() ? nullptr : &*found;
}

const GalleryPageCounts& GalleryPageModel::counts() const noexcept {
    return counts_;
}

const GalleryPageRoute& GalleryPageModel::current() const noexcept {
    return current_;
}

const std::string& GalleryPageModel::query() const noexcept {
    return query_;
}

GallerySupportFilter GalleryPageModel::filter() const noexcept {
    return filter_;
}

bool GalleryPageModel::valid(const GalleryPageRoute& route) const noexcept {
    if (!valid_family(route.family) || (route.section && (*route.section < GalleryPageSection::usage ||
                                                          *route.section > GalleryPageSection::sources))) {
        return false;
    }
    if (route.kind == GalleryPageKind::overview || route.kind == GalleryPageKind::introduction ||
        route.kind == GalleryPageKind::tokens) {
        return route.component_identity.empty() && route.family == GalleryInputFamily::input &&
               route.sample_identity.empty();
    }
    if (route.kind != GalleryPageKind::component || !find(route.component_identity)) {
        return false;
    }
    return route.component_identity == "ant.component.input" || route.family == GalleryInputFamily::input;
}

bool GalleryPageModel::select(GalleryPageRoute route) {
    if (!valid(route)) {
        throw std::invalid_argument("Gallery page route is not registered");
    }
    if (route == current_) {
        return false;
    }
    current_ = std::move(route);
    return true;
}

bool GalleryPageModel::set_query(std::string value) {
    if (value == query_) {
        return false;
    }
    query_ = std::move(value);
    return true;
}

bool GalleryPageModel::set_filter(GallerySupportFilter value) noexcept {
    if (value < GallerySupportFilter::all || value > GallerySupportFilter::out_of_scope || value == filter_) {
        return false;
    }
    filter_ = value;
    return true;
}

std::vector<GalleryPageSearchResult> GalleryPageModel::results() const {
    const auto needle = folded(query_);
    std::vector<GalleryPageSearchResult> result;
    for (const auto& page : pages_) {
        const auto& entry = *page.entry;
        if (!gallery_support_filter_matches(filter_, entry.support_status)) {
            continue;
        }
        bool matches = needle.empty() || folded(entry.english_name).find(needle) != std::string::npos ||
                       entry.chinese_name.find(needle) != std::string_view::npos ||
                       entry.identity.find(needle) != std::string_view::npos;
        auto family = GalleryInputFamily::input;
        if (entry.identity == "ant.component.input" && !needle.empty()) {
            for (const auto& alias : aliases) {
                if (alias.english.find(needle) != std::string_view::npos ||
                    alias.chinese.find(needle) != std::string_view::npos) {
                    matches = true;
                    family = alias.family;
                    break;
                }
            }
        }
        if (matches) {
            result.push_back({&page, family});
        }
    }
    return result;
}

bool GalleryPageModel::current_matches() const {
    if (current_.kind != GalleryPageKind::component) {
        return true;
    }
    return std::ranges::any_of(
        results(), [this](const auto& result) { return result.page->entry->identity == current_.component_identity; });
}

std::string_view gallery_input_family_name(GalleryInputFamily family) noexcept {
    switch (family) {
    case GalleryInputFamily::input:
        return "Input";
    case GalleryInputFamily::search:
        return "Search";
    case GalleryInputFamily::password:
        return "Password";
    case GalleryInputFamily::text_area:
        return "TextArea";
    case GalleryInputFamily::otp:
        return "OTP";
    }
    return {};
}

std::string_view gallery_page_status_label(GallerySupportStatus status) noexcept {
    switch (status) {
    case GallerySupportStatus::implemented:
        return "已实现（声明的原生范围）";
    case GallerySupportStatus::partial:
        return "部分支持";
    case GallerySupportStatus::planned:
        return "未实现";
    case GallerySupportStatus::web_only:
        return "仅 Web";
    case GallerySupportStatus::deprecated:
        return "已弃用";
    case GallerySupportStatus::out_of_scope:
        return "范围外";
    }
    return "未知状态";
}

std::string_view gallery_page_empty_message(GallerySupportStatus status) noexcept {
    switch (status) {
    case GallerySupportStatus::planned:
        return "尚未提供稳定原生组件。此页保留参考信息与缺失范围。";
    case GallerySupportStatus::deprecated:
        return "此条目已弃用，仅保留参考说明。";
    case GallerySupportStatus::web_only:
        return "此能力属于 Web 范围，未提供原生演示。";
    case GallerySupportStatus::out_of_scope:
        return "此能力不在当前原生实现范围。";
    case GallerySupportStatus::implemented:
    case GallerySupportStatus::partial:
        return "本页尚无已登记样例，可通过关联入口查看已有原生流程。";
    }
    return {};
}

} // namespace rynui::example
