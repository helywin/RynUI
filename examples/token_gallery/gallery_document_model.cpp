#include "gallery_document_model.hpp"

#include <array>

namespace rynui::example {
namespace {

constexpr std::array sections{
    GalleryDocumentSection{
        GalleryDocumentSectionKind::header_source,
        "gallery.document.header-source",
        "RynUI Ant Design Reference",
        "RynUI 的离线实现参考，锁定 Ant Design 6.5.0 与对应源码提交。",
    },
    GalleryDocumentSection{
        GalleryDocumentSectionKind::introduction,
        "gallery.document.introduction",
        "Introduction / 设计介绍",
        "面向桌面 C++ 的企业级设计系统基线，强调清晰、一致、可验证与可演进。",
    },
    GalleryDocumentSection{
        GalleryDocumentSectionKind::design_values,
        "gallery.document.design-values",
        "Design Values / 设计价值",
        "四项价值用于解释取舍，不替代组件合同、Design Token 或平台验收。",
    },
    GalleryDocumentSection{
        GalleryDocumentSectionKind::foundation_tokens,
        "gallery.document.foundation-tokens",
        "Foundation & Design Token / 基础与令牌",
        "颜色、排版、间距、圆角与阴影来自锁定 Token，不用交互控件模拟说明内容。",
    },
    GalleryDocumentSection{
        GalleryDocumentSectionKind::component_overview,
        "gallery.document.component-overview",
        "Component Overview / 组件总览",
        "七类 72 项完整列出，并区分已支持子集、缺失范围与可定位证据。",
    },
    GalleryDocumentSection{
        GalleryDocumentSectionKind::live_samples,
        "gallery.document.live-samples",
        "Live Samples / 真实样例",
        "只有 RynUI 已提供的真实组件进入交互树；目录与说明保持非交互。",
    },
};

constexpr std::array values{
    GalleryDesignValue{
        "gallery.value.natural",
        "Natural",
        "自然",
        "界面顺应用户已有认知，让操作结果容易预期。",
    },
    GalleryDesignValue{
        "gallery.value.certain",
        "Certain",
        "确定",
        "状态、层级和反馈保持一致，减少模糊与猜测。",
    },
    GalleryDesignValue{
        "gallery.value.meaningful",
        "Meaningful",
        "有意义",
        "视觉强调服务于任务与信息，不增加无目的装饰。",
    },
    GalleryDesignValue{
        "gallery.value.growing",
        "Growing",
        "生长",
        "组件、Token 与验收证据能够随产品规模稳定扩展。",
    },
};

} // namespace

std::span<const GalleryDocumentSection>
gallery_document_sections() noexcept {
    return sections;
}

std::span<const GalleryDesignValue>
gallery_design_values() noexcept {
    return values;
}

bool gallery_support_filter_matches(
    GallerySupportFilter filter,
    GallerySupportStatus status) noexcept {
    switch (filter) {
    case GallerySupportFilter::all:
        return true;
    case GallerySupportFilter::implemented:
        return status == GallerySupportStatus::implemented;
    case GallerySupportFilter::partial:
        return status == GallerySupportStatus::partial;
    case GallerySupportFilter::planned:
        return status == GallerySupportStatus::planned;
    case GallerySupportFilter::web_only:
        return status == GallerySupportStatus::web_only;
    case GallerySupportFilter::deprecated:
        return status == GallerySupportStatus::deprecated;
    case GallerySupportFilter::out_of_scope:
        return status == GallerySupportStatus::out_of_scope;
    }
    return false;
}

std::string_view gallery_category_title(
    AntDesignGalleryCategory category) noexcept {
    switch (category) {
    case AntDesignGalleryCategory::general:
        return "General / 通用";
    case AntDesignGalleryCategory::layout:
        return "Layout / 布局";
    case AntDesignGalleryCategory::navigation:
        return "Navigation / 导航";
    case AntDesignGalleryCategory::data_entry:
        return "Data Entry / 数据录入";
    case AntDesignGalleryCategory::data_display:
        return "Data Display / 数据展示";
    case AntDesignGalleryCategory::feedback:
        return "Feedback / 反馈";
    case AntDesignGalleryCategory::other:
        return "Other / 其他";
    }
    return "Invalid / 无效";
}

} // namespace rynui::example
