#pragma once

#include <ryn/rynui.hpp>

namespace i18n_example {

// A small public-API application definition, also mounted by the CPU fixture.
// Embedders pass content(model) to their normal RynUI window host.
struct Model final {
    ryn::Signal<ryn::LocaleId> language{ryn::locales::zh_CN};
    ryn::Signal<ryn::LocaleId> format_language{ryn::locales::zh_CN};
    ryn::Signal<ryn::TextDirection> direction{ryn::TextDirection::LeftToRight};
    ryn::Signal<std::uint64_t> selected{2};
    ryn::Signal<ryn::MessageCatalog> messages;
    ryn::InputRef editor;
    ryn::InputRef popup_editor;
    ryn::Signal<bool> popup_open{false};
    ryn::I18n outer;
    ryn::I18n inner;
    unsigned mount_runs{};
    unsigned edits{};
    unsigned rich_mount_runs{};

    Model();
};

[[nodiscard]] ryn::Content content(std::shared_ptr<Model> model);

} // namespace i18n_example
