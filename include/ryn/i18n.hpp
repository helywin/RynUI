#pragma once

#include <ryn/component_locales.hpp>
#include <ryn/decimal.hpp>
#include <ryn/prop.hpp>

#include <memory>
#include <compare>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ryn {

class LocaleId final {
public:
    LocaleId();
    explicit LocaleId(std::string_view value);
    [[nodiscard]] const std::string& value() const noexcept;
    friend bool operator==(const LocaleId&, const LocaleId&) = default;
    friend auto operator<=>(const LocaleId&, const LocaleId&) = default;

private:
    std::string value_;
};

namespace locales {
inline const LocaleId en_US{"en-US"};
inline const LocaleId zh_CN{"zh-CN"};
} // namespace locales

enum class I18nErrorCode {
    InvalidLocale,
    UnknownLocale,
    InvalidPack,
    InvalidFallback,
    Duplicate,
    MissingKey,
    InvalidSchema,
    MalformedTemplate,
    MissingArgument,
    ExtraArgument,
    ArgumentType,
    UnsupportedPlural,
    UnsupportedFormat,
    StaleContext,
};

// Contains identities/categories only; never stores argument values or message bodies.
struct I18nError final {
    I18nErrorCode code{};
    LocaleId locale;
    String key;
    friend bool operator==(const I18nError&, const I18nError&) = default;
};

class I18nException final : public std::invalid_argument {
public:
    explicit I18nException(I18nError error);
    [[nodiscard]] const I18nError& error() const noexcept;

private:
    I18nError error_;
};

enum class IntegerPluralRule { EnglishOneOther, OtherOnly, Unsupported };

// Actual supported format data. No timezone, currency or general Intl promise.
struct LocaleFormatSupport final {
    LocaleId source;
    bool decimal{true};
    bool gregorian{true};
    friend bool operator==(const LocaleFormatSupport&, const LocaleFormatSupport&) = default;
};

struct LocaleData final {
    CommonLocale common;
    PickerLocale picker;
    PaginationLocale pagination;
    TransferLocale transfer;
    UploadLocale upload;
    ImageLocale image;
    FormLocale form;
    DescriptionsLocale descriptions;
    QRCodeLocale qrcode;
    StatisticLocale statistic;
    TableLocale table;
    CascaderLocale cascader;
    ColorPickerLocale color_picker;
    IntegerPluralRule plural{IntegerPluralRule::Unsupported};
    LocaleFormatSupport formats;
    friend bool operator==(const LocaleData&, const LocaleData&) = default;
};

class LocalePack final {
public:
    explicit LocalePack(LocaleData data);
    [[nodiscard]] const LocaleData& data() const noexcept;
    friend bool operator==(const LocalePack& left, const LocalePack& right);

private:
    std::shared_ptr<const LocaleData> data_;
};

struct LocaleDefinition final {
    LocaleId id;
    LocalePack pack;
    std::optional<LocaleId> fallback{locales::en_US};
    friend bool operator==(const LocaleDefinition&, const LocaleDefinition&) = default;
};

struct LocaleAlias final {
    LocaleId id;
    LocaleId target;
    friend bool operator==(const LocaleAlias&, const LocaleAlias&) = default;
};

struct LocaleResolution final {
    LocaleId requested;
    LocaleId effective;
    LocalePack pack;
    std::vector<LocaleId> candidates;
    std::optional<I18nError> diagnostic;
    friend bool operator==(const LocaleResolution&, const LocaleResolution&) = default;
};

class LocaleCatalog final {
public:
    // Includes en-US, zh-CN and explicit en/zh/zh-Hans aliases. Custom definitions
    // extend these immutable defaults; duplicate identities are rejected.
    LocaleCatalog();
    explicit LocaleCatalog(std::vector<LocaleDefinition> definitions, std::vector<LocaleAlias> aliases = {},
                           LocaleId fallback = locales::en_US);
    [[nodiscard]] LocaleResolution resolve(const LocaleId& requested) const;
    [[nodiscard]] LocaleResolution resolveFormat(const LocaleId& requested) const;
    [[nodiscard]] const LocalePack& pack(const LocaleId& exact_id) const;
    [[nodiscard]] static const LocaleCatalog& builtin();
    friend bool operator==(const LocaleCatalog& left, const LocaleCatalog& right);

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

enum class MessageArgType { Text, Int, UInt, Decimal };

struct MessageParameter final {
    String name;
    MessageArgType type{MessageArgType::Text};
    friend bool operator==(const MessageParameter&, const MessageParameter&) = default;
};

struct MessageArg final {
    using Value = std::variant<String, std::int64_t, std::uint64_t, Decimal>;
    String name;
    Value value;
    friend bool operator==(const MessageArg&, const MessageArg&) = default;
};

using MessageArgs = std::vector<MessageArg>;

struct PluralMessage final {
    String count;
    std::optional<String> one;
    std::optional<String> other;
    friend bool operator==(const PluralMessage&, const PluralMessage&) = default;
};

using MessagePattern = std::variant<String, PluralMessage>;

class MessageKey final {
public:
    MessageKey(String id, MessagePattern english_fallback, std::vector<MessageParameter> schema = {});
    [[nodiscard]] const String& id() const noexcept;
    [[nodiscard]] const MessagePattern& fallback() const noexcept;
    [[nodiscard]] std::span<const MessageParameter> schema() const noexcept;
    friend bool operator==(const MessageKey& left, const MessageKey& right);

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

struct MessageTranslation final {
    LocaleId locale;
    MessageKey key;
    MessagePattern pattern;
    friend bool operator==(const MessageTranslation&, const MessageTranslation&) = default;
};

class MessageCatalog final {
public:
    MessageCatalog();
    explicit MessageCatalog(std::vector<MessageTranslation> translations);
    [[nodiscard]] const MessageTranslation* find(const LocaleId& locale, StringView id) const noexcept;
    [[nodiscard]] std::span<const MessageTranslation> translations() const noexcept;
    friend bool operator==(const MessageCatalog& left, const MessageCatalog& right);

private:
    std::shared_ptr<const std::vector<MessageTranslation>> translations_;
};

using TranslationResult = std::variant<String, I18nError>;
using MessageLookupResult = std::variant<MessageTranslation, I18nError>;

// Layers are nearest first. Each candidate locale visits ALL layers before
// advancing to fallback, so a parent's current language cannot leak into a child.
[[nodiscard]] MessageLookupResult lookupMessage(const LocaleCatalog& locales, const LocaleId& requested, StringView id,
                                                std::span<const MessageCatalog> layers);
[[nodiscard]] String formatMessage(const MessageKey& key, const MessagePattern& pattern, const MessageArgs& args,
                                   IntegerPluralRule rule);
[[nodiscard]] TranslationResult tryTranslate(const LocaleCatalog& locales, const LocaleId& requested,
                                             const MessageKey& key, const MessageArgs& args = {},
                                             std::span<const MessageCatalog> layers = {});
[[nodiscard]] String translate(const LocaleCatalog& locales, const LocaleId& requested, const MessageKey& key,
                               const MessageArgs& args = {}, std::span<const MessageCatalog> layers = {});

namespace runtime {
class ConfigScope;
struct I18nLifetime;
} // namespace runtime

class I18n final {
public:
    I18n() = default;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] LocaleResolution locale() const;
    [[nodiscard]] LocaleResolution formatLocale() const;
    [[nodiscard]] TranslationResult tryTranslate(const MessageKey& key, const MessageArgs& args = {}) const;
    [[nodiscard]] String translate(const MessageKey& key, const MessageArgs& args = {}) const;
    [[nodiscard]] Binding<String> text(MessageKey key, Prop<MessageArgs> args = MessageArgs{}) const;

private:
    friend I18n useI18n();
    I18n(std::weak_ptr<runtime::ConfigScope> scope, std::weak_ptr<runtime::I18nLifetime> lifetime);
    std::weak_ptr<runtime::ConfigScope> scope_;
    std::weak_ptr<runtime::I18nLifetime> lifetime_;
};

// Mount-time capture; subsequent event calls read the captured live scope.
[[nodiscard]] I18n useI18n();

} // namespace ryn
