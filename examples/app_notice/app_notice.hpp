#pragma once

#include <ryn/app.hpp>
#include <ryn/config_provider.hpp>
#include <ryn/i18n.hpp>
#include <ryn/input.hpp>

namespace app_notice_example {
struct Model final {
    ryn::Signal<ryn::LocaleId> language{ryn::locales::zh_CN};
    ryn::AppRef app;
    ryn::I18n i18n;
    ryn::InputRef editor;
    ryn::NoticeHandle notice;
    unsigned content_mounts{};
    unsigned notice_mounts{};

    void open();
    void update();
    void close();
};

[[nodiscard]] ryn::Content content(std::shared_ptr<Model> model);
} // namespace app_notice_example
