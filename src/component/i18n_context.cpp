#include "runtime/component_host.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/i18n.hpp>

namespace ryn::runtime {

struct I18nLifetime final {
    bool active{true};
    std::vector<std::shared_ptr<void>> projections;
};

} // namespace ryn::runtime

namespace ryn {
namespace {

std::shared_ptr<runtime::ConfigScope> lock_scope(const std::weak_ptr<runtime::ConfigScope>& weak,
                                                 const std::weak_ptr<runtime::I18nLifetime>& owner) {
    const auto scope = weak.lock();
    const auto lifetime = owner.lock();
    if (!scope || !lifetime || !lifetime->active) {
        throw I18nException({I18nErrorCode::StaleContext, {}, {}});
    }
    scope->ensure_live();
    return scope;
}

} // namespace

I18n::I18n(std::weak_ptr<runtime::ConfigScope> scope, std::weak_ptr<runtime::I18nLifetime> lifetime)
    : scope_(std::move(scope)), lifetime_(std::move(lifetime)) {}

bool I18n::active() const noexcept {
    const auto scope = scope_.lock();
    const auto lifetime = lifetime_.lock();
    return scope && scope->active() && lifetime && lifetime->active;
}

LocaleResolution I18n::locale() const {
    return lock_scope(scope_, lifetime_)->snapshot().locale;
}

LocaleResolution I18n::formatLocale() const {
    return lock_scope(scope_, lifetime_)->snapshot().format_locale;
}

TranslationResult I18n::tryTranslate(const MessageKey& key, const MessageArgs& args) const {
    try {
        const auto scope = lock_scope(scope_, lifetime_);
        const auto& snapshot = scope->snapshot();
        return ryn::tryTranslate(snapshot.catalog, snapshot.locale.requested, key, args, snapshot.messages);
    } catch (const I18nException& error) {
        return error.error();
    }
}

String I18n::translate(const MessageKey& key, const MessageArgs& args) const {
    auto result = tryTranslate(key, args);
    if (const auto* error = std::get_if<I18nError>(&result)) {
        throw I18nException(*error);
    }
    return std::get<String>(std::move(result));
}

Binding<String> I18n::text(MessageKey key, Prop<MessageArgs> args) const {
    static_cast<void>(lock_scope(scope_, lifetime_));
    auto memo = std::make_shared<Memo<String>>(
        [weak = scope_, owner = lifetime_, key = std::move(key), args = std::move(args)] {
            const auto live = lock_scope(weak, owner);
            const auto values = detail::read_prop(args);
            // User-supplied Binding evaluation can dispose/reconfigure its owner.
            static_cast<void>(lock_scope(weak, owner));
            const auto& snapshot = live->snapshot();
            return ryn::translate(snapshot.catalog, snapshot.locale.requested, key, values, snapshot.messages);
        });
    static_cast<void>(lock_scope(scope_, lifetime_));
    lifetime_.lock()->projections.push_back(memo);
    const std::weak_ptr<Memo<String>> weak_memo = memo;
    return bind([weak = scope_, owner = lifetime_, weak_memo] {
        static_cast<void>(lock_scope(weak, owner));
        const auto live_memo = weak_memo.lock();
        if (!live_memo) {
            throw I18nException({I18nErrorCode::StaleContext, {}, {}});
        }
        return live_memo->get();
    });
}

I18n useI18n() {
    auto& build = runtime::require_component_build_context();
    auto lifetime = std::make_shared<runtime::I18nLifetime>();
    build.lifetime_scope().on_cleanup([lifetime] {
        lifetime->active = false;
        lifetime->projections.clear();
    });
    return I18n(build.config_scope(), lifetime);
}

} // namespace ryn
