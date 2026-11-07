#pragma once

#include <ryn/control_size.hpp>
#include <ryn/i18n.hpp>
#include <ryn/text_direction.hpp>

#include <thread>

namespace ryn::runtime {

struct ConfigOverrides final {
    std::optional<LocaleId> locale;
    std::optional<LocaleId> format_locale;
    std::optional<LocaleCatalog> catalog;
    std::optional<MessageCatalog> messages;
    std::optional<TextDirection> direction;
    std::optional<ControlSize> size;
    std::optional<bool> disabled;
    friend bool operator==(const ConfigOverrides&, const ConfigOverrides&) = default;
};

struct ConfigSnapshot final {
    LocaleCatalog catalog;
    LocaleResolution locale;
    LocaleResolution format_locale;
    std::optional<LocaleId> explicit_format_locale;
    std::vector<MessageCatalog> messages;
    TextDirection direction{TextDirection::LeftToRight};
    ControlSize size{ControlSize::Middle};
    bool disabled{};
    std::optional<TextDirection> explicit_direction;
    std::optional<ControlSize> explicit_size;
    friend bool operator==(const ConfigSnapshot&, const ConfigSnapshot&) = default;
};

struct ConfigSnapshotEqual final {
    bool operator()(const std::shared_ptr<const ConfigSnapshot>& left,
                    const std::shared_ptr<const ConfigSnapshot>& right) const {
        return left == right || *left == *right;
    }
};

class ConfigScope final : public std::enable_shared_from_this<ConfigScope> {
public:
    [[nodiscard]] static std::shared_ptr<ConfigScope> create_default();
    [[nodiscard]] static std::shared_ptr<ConfigScope> create(std::shared_ptr<ConfigScope> parent,
                                                             ConfigOverrides overrides);
    ~ConfigScope();
    bool update(ConfigOverrides overrides);
    void dispose() noexcept;
    [[nodiscard]] bool active() const noexcept;
    void ensure_live() const;
    [[nodiscard]] const ConfigSnapshot& snapshot() const;
    [[nodiscard]] std::uint64_t generation() const;

private:
    struct Prepared;
    ConfigScope(std::shared_ptr<ConfigScope> parent, ConfigOverrides overrides,
                std::shared_ptr<const ConfigSnapshot> snapshot);
    static std::shared_ptr<const ConfigSnapshot> resolve(const ConfigSnapshot* parent,
                                                         const ConfigOverrides& overrides);
    void prepare(const ConfigOverrides& overrides, const ConfigSnapshot* parent, std::vector<Prepared>& prepared);

    std::shared_ptr<ConfigScope> parent_;
    ConfigOverrides overrides_;
    std::shared_ptr<const ConfigSnapshot> current_;
    Signal<std::shared_ptr<const ConfigSnapshot>, ConfigSnapshotEqual> source_;
    std::vector<std::weak_ptr<ConfigScope>> children_;
    std::thread::id owner_thread_;
    std::uint64_t generation_{1};
    bool active_{true};
};

} // namespace ryn::runtime
