#include "runtime/config_context.hpp"

#include <algorithm>

namespace ryn::runtime {

struct ConfigScope::Prepared final {
    std::shared_ptr<ConfigScope> scope;
    std::shared_ptr<const ConfigSnapshot> snapshot;
    std::uint64_t generation{};
};

std::shared_ptr<const ConfigSnapshot> ConfigScope::resolve(const ConfigSnapshot* parent,
                                                           const ConfigOverrides& overrides) {
    const auto catalog = overrides.catalog.value_or(parent ? parent->catalog : LocaleCatalog{});
    const auto locale = catalog.resolve(overrides.locale.value_or(parent ? parent->locale.requested : locales::en_US));
    const auto format_request =
        overrides.format_locale ? overrides.format_locale : (parent ? parent->explicit_format_locale : std::nullopt);
    auto format = catalog.resolveFormat(format_request.value_or(locale.requested));
    auto messages = parent ? parent->messages : std::vector<MessageCatalog>{};
    if (overrides.messages) {
        messages.insert(messages.begin(), *overrides.messages);
    }
    for (std::size_t i = 0; i < messages.size(); ++i) {
        for (const auto& entry : messages[i].translations()) {
            const auto& pack = catalog.pack(entry.locale);
            if (const auto* plural = std::get_if<PluralMessage>(&entry.pattern)) {
                if (pack.data().plural == IntegerPluralRule::Unsupported) {
                    throw I18nException({I18nErrorCode::UnsupportedPlural, entry.locale, entry.key.id()});
                }
                if (pack.data().plural == IntegerPluralRule::EnglishOneOther && !plural->one) {
                    throw I18nException({I18nErrorCode::InvalidSchema, entry.locale, entry.key.id()});
                }
            }
            for (std::size_t j = 0; j < i; ++j) {
                for (const auto& prior : messages[j].translations()) {
                    if (prior.key.id() == entry.key.id() && prior.key != entry.key) {
                        throw I18nException({I18nErrorCode::InvalidSchema, entry.locale, entry.key.id()});
                    }
                }
            }
        }
    }
    const auto direction = overrides.direction.value_or(parent ? parent->direction : TextDirection::LeftToRight);
    const auto size = overrides.size.value_or(parent ? parent->size : ControlSize::Middle);
    if ((direction != TextDirection::Auto && direction != TextDirection::LeftToRight &&
         direction != TextDirection::RightToLeft) ||
        (size != ControlSize::Small && size != ControlSize::Middle && size != ControlSize::Large)) {
        throw I18nException({I18nErrorCode::InvalidSchema, locale.requested, {}});
    }
    return std::make_shared<const ConfigSnapshot>(
        ConfigSnapshot{catalog, locale, std::move(format), format_request, std::move(messages), direction, size,
                       overrides.disabled.value_or(parent ? parent->disabled : false)});
}

ConfigScope::ConfigScope(std::shared_ptr<ConfigScope> parent, ConfigOverrides overrides,
                         std::shared_ptr<const ConfigSnapshot> snapshot)
    : parent_(std::move(parent)), overrides_(std::move(overrides)), current_(std::move(snapshot)), source_(current_),
      owner_thread_(std::this_thread::get_id()) {}

std::shared_ptr<ConfigScope> ConfigScope::create_default() {
    return std::shared_ptr<ConfigScope>(new ConfigScope(nullptr, {}, resolve(nullptr, {})));
}

std::shared_ptr<ConfigScope> ConfigScope::create(std::shared_ptr<ConfigScope> parent, ConfigOverrides overrides) {
    if (!parent) {
        throw I18nException({I18nErrorCode::StaleContext, {}, {}});
    }
    parent->ensure_live();
    const auto snapshot = resolve(parent->current_.get(), overrides);
    auto child = std::shared_ptr<ConfigScope>(new ConfigScope(parent, std::move(overrides), snapshot));
    std::erase_if(parent->children_, [](const auto& weak) { return weak.expired(); });
    parent->children_.push_back(child);
    return child;
}

ConfigScope::~ConfigScope() {
    dispose();
}

void ConfigScope::prepare(const ConfigOverrides& overrides, const ConfigSnapshot* parent,
                          std::vector<Prepared>& prepared) {
    ensure_live();
    const auto next = resolve(parent, overrides);
    prepared.push_back({shared_from_this(), next, generation_});
    for (const auto& weak : children_) {
        if (const auto child = weak.lock(); child && child->active()) {
            child->prepare(child->overrides_, next.get(), prepared);
        }
    }
}

bool ConfigScope::update(ConfigOverrides overrides) {
    ensure_live();
    if (overrides_ == overrides) {
        return false;
    }
    std::vector<Prepared> prepared;
    prepare(overrides, parent_ ? parent_->current_.get() : nullptr, prepared);
    // No callback ran during preflight. Keep configuration provenance even when
    // today's inherited values happen to equal an explicit override.
    overrides_ = std::move(overrides);
    bool changed = false;
    for (auto& entry : prepared) {
        if (*entry.scope->current_ != *entry.snapshot) {
            entry.scope->current_ = entry.snapshot;
            entry.generation = ++entry.scope->generation_;
            changed = true;
        }
    }
    batch([&] {
        for (const auto& entry : prepared) {
            if (entry.scope->active() && entry.scope->generation_ == entry.generation) {
                entry.scope->source_.set(entry.scope->current_);
            }
        }
    });
    return changed;
}

void ConfigScope::dispose() noexcept {
    if (!active_) {
        return;
    }
    active_ = false;
    ++generation_;
    for (const auto& weak : children_) {
        if (const auto child = weak.lock()) {
            child->dispose();
        }
    }
    children_.clear();
}

bool ConfigScope::active() const noexcept {
    return active_;
}

void ConfigScope::ensure_live() const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error("ConfigScope can only be used on its owner thread");
    }
    if (!active_) {
        throw I18nException({I18nErrorCode::StaleContext, {}, {}});
    }
}

const ConfigSnapshot& ConfigScope::snapshot() const {
    ensure_live();
    return *source_.get();
}

std::uint64_t ConfigScope::generation() const {
    ensure_live();
    return generation_;
}

} // namespace ryn::runtime
