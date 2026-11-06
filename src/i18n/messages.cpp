#include <ryn/i18n.hpp>

#include <algorithm>
#include <charconv>
#include <set>
#include <type_traits>

namespace ryn {
namespace {

[[noreturn]] void fail(I18nErrorCode code, const String& key = {}) {
    throw I18nException({code, locales::en_US, key});
}

bool name_valid(std::string_view name, bool key = false) {
    if (name.empty()) {
        return false;
    }
    for (const char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
              (key && (c == '.' || c == '-')))) {
            return false;
        }
    }
    return true;
}

String string_from(std::string_view text) {
    return String::from_utf8(text).value();
}

// The visitor sees source template spans only. Inserted strings never return
// through the parser, even if they contain braces or another placeholder.
template <typename Literal, typename Argument>
void visit_template(const String& text, const String& key, Literal literal, Argument argument) {
    const auto source = text.bytes();
    std::size_t position = 0;
    while (position < source.size()) {
        const auto start = position;
        while (position < source.size() && source[position] != '{' && source[position] != '}') {
            ++position;
        }
        literal(source.substr(start, position - start));
        if (position == source.size()) {
            break;
        }
        const char brace = source[position++];
        if (position < source.size() && source[position] == brace) {
            literal(source.substr(position++, 1));
            continue;
        }
        if (brace == '}') {
            fail(I18nErrorCode::MalformedTemplate, key);
        }
        const auto end = source.find('}', position);
        if (end == source.npos || !name_valid(source.substr(position, end - position))) {
            fail(I18nErrorCode::MalformedTemplate, key);
        }
        argument(source.substr(position, end - position));
        position = end + 1;
    }
}

void validate_pattern(const String& id, const MessagePattern& pattern, std::span<const MessageParameter> schema) {
    const auto* plural = std::get_if<PluralMessage>(&pattern);
    if (plural) {
        const auto selector = std::find_if(schema.begin(), schema.end(), [&](const auto& p) {
            return p.name == plural->count && p.type == MessageArgType::UInt;
        });
        if (selector == schema.end() || !plural->other) {
            fail(I18nErrorCode::InvalidSchema, id);
        }
    }
    const auto check = [&](const String& text) {
        std::set<std::string> seen;
        visit_template(
            text, id, [](auto) {},
            [&](std::string_view name) {
                const auto parameter =
                    std::find_if(schema.begin(), schema.end(), [&](const auto& p) { return p.name.bytes() == name; });
                if (parameter == schema.end()) {
                    fail(I18nErrorCode::InvalidSchema, id);
                }
                seen.emplace(name);
            });
        for (const auto& p : schema) {
            if (!seen.contains(std::string(p.name.bytes())) && (!plural || p.name != plural->count)) {
                fail(I18nErrorCode::InvalidSchema, id);
            }
        }
    };
    if (plural) {
        check(*plural->other);
        if (plural->one) {
            check(*plural->one);
        }
    } else {
        check(std::get<String>(pattern));
    }
}

void validate_args(const MessageKey& key, const MessageArgs& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        for (std::size_t j = 0; j < i; ++j) {
            if (arg.name == args[j].name) {
                fail(I18nErrorCode::Duplicate, key.id());
            }
        }
        const auto schema = key.schema();
        const auto p = std::find_if(schema.begin(), schema.end(), [&](const auto& s) { return s.name == arg.name; });
        if (p == schema.end()) {
            fail(I18nErrorCode::ExtraArgument, key.id());
        }
        if (static_cast<std::size_t>(p->type) != arg.value.index()) {
            fail(I18nErrorCode::ArgumentType, key.id());
        }
    }
    if (args.size() != key.schema().size()) {
        fail(I18nErrorCode::MissingArgument, key.id());
    }
}

std::string argument_text(const MessageArg::Value& value) {
    return std::visit(
        [](const auto& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, String>) {
                return std::string(arg.bytes());
            } else if constexpr (std::is_same_v<T, Decimal>) {
                return std::string(arg.text().bytes());
            } else {
                char buffer[32];
                const auto result = std::to_chars(std::begin(buffer), std::end(buffer), arg);
                return {buffer, result.ptr};
            }
        },
        value);
}

} // namespace

I18nException::I18nException(I18nError error)
    : std::invalid_argument("Invalid i18n configuration or translation; inspect the typed error"),
      error_(std::move(error)) {}

const I18nError& I18nException::error() const noexcept {
    return error_;
}

struct MessageKey::Data final {
    String id;
    MessagePattern fallback;
    std::vector<MessageParameter> schema;
    friend bool operator==(const Data&, const Data&) = default;
};

MessageKey::MessageKey(String id, MessagePattern english_fallback, std::vector<MessageParameter> schema) {
    if (!name_valid(id.bytes(), true)) {
        fail(I18nErrorCode::InvalidSchema);
    }
    std::sort(schema.begin(), schema.end(),
              [](const auto& a, const auto& b) { return a.name.bytes() < b.name.bytes(); });
    for (std::size_t i = 0; i < schema.size(); ++i) {
        if (!name_valid(schema[i].name.bytes()) || (i && schema[i].name == schema[i - 1].name) ||
            static_cast<unsigned>(schema[i].type) > static_cast<unsigned>(MessageArgType::Decimal)) {
            fail(I18nErrorCode::InvalidSchema, id);
        }
    }
    validate_pattern(id, english_fallback, schema);
    if (const auto* plural = std::get_if<PluralMessage>(&english_fallback); plural && !plural->one) {
        fail(I18nErrorCode::InvalidSchema, id);
    }
    data_ = std::make_shared<const Data>(Data{std::move(id), std::move(english_fallback), std::move(schema)});
}

const String& MessageKey::id() const noexcept {
    return data_->id;
}

const MessagePattern& MessageKey::fallback() const noexcept {
    return data_->fallback;
}

std::span<const MessageParameter> MessageKey::schema() const noexcept {
    return data_->schema;
}

bool operator==(const MessageKey& left, const MessageKey& right) {
    return left.data_ == right.data_ || *left.data_ == *right.data_;
}

MessageCatalog::MessageCatalog() : MessageCatalog(std::vector<MessageTranslation>{}) {}

MessageCatalog::MessageCatalog(std::vector<MessageTranslation> translations) {
    std::sort(translations.begin(), translations.end(), [](const auto& a, const auto& b) {
        if (a.key.id() != b.key.id()) {
            return a.key.id().bytes() < b.key.id().bytes();
        }
        return a.locale < b.locale;
    });
    for (std::size_t i = 0; i < translations.size(); ++i) {
        const auto& entry = translations[i];
        validate_pattern(entry.key.id(), entry.pattern, entry.key.schema());
        if (const auto* plural = std::get_if<PluralMessage>(&entry.pattern);
            plural && entry.locale == locales::en_US && !plural->one) {
            fail(I18nErrorCode::InvalidSchema, entry.key.id());
        }
        if (entry.pattern.index() != entry.key.fallback().index()) {
            fail(I18nErrorCode::InvalidSchema, entry.key.id());
        }
        if (i && translations[i - 1].key.id() == entry.key.id()) {
            if (translations[i - 1].key != entry.key) {
                fail(I18nErrorCode::InvalidSchema, entry.key.id());
            }
            if (translations[i - 1].locale == entry.locale) {
                fail(I18nErrorCode::Duplicate, entry.key.id());
            }
        }
    }
    translations_ = std::make_shared<const std::vector<MessageTranslation>>(std::move(translations));
}

const MessageTranslation* MessageCatalog::find(const LocaleId& locale, StringView id) const noexcept {
    const auto entry = std::lower_bound(translations_->begin(), translations_->end(), id,
                                        [](const auto& a, StringView b) { return a.key.id().bytes() < b.bytes(); });
    for (auto it = entry; it != translations_->end() && it->key.id().view() == id; ++it) {
        if (it->locale == locale) {
            return &*it;
        }
    }
    return nullptr;
}

std::span<const MessageTranslation> MessageCatalog::translations() const noexcept {
    return *translations_;
}

bool operator==(const MessageCatalog& left, const MessageCatalog& right) {
    return left.translations_ == right.translations_ || *left.translations_ == *right.translations_;
}

MessageLookupResult lookupMessage(const LocaleCatalog& catalog, const LocaleId& requested, StringView id,
                                  std::span<const MessageCatalog> layers) {
    if (!name_valid(id.bytes(), true)) {
        return I18nError{I18nErrorCode::InvalidSchema, requested, {}};
    }
    for (const auto& candidate : catalog.resolve(requested).candidates) {
        for (const auto& layer : layers) {
            if (const auto* entry = layer.find(candidate, id)) {
                return *entry;
            }
        }
    }
    return I18nError{I18nErrorCode::MissingKey, requested, string_from(id.bytes())};
}

String formatMessage(const MessageKey& key, const MessagePattern& pattern, const MessageArgs& args,
                     IntegerPluralRule rule) {
    validate_pattern(key.id(), pattern, key.schema());
    if (pattern.index() != key.fallback().index()) {
        fail(I18nErrorCode::InvalidSchema, key.id());
    }
    validate_args(key, args);
    const auto arg = [&](std::string_view name) -> const MessageArg::Value& {
        return std::find_if(args.begin(), args.end(), [&](const auto& a) { return a.name.bytes() == name; })->value;
    };
    const String* text = std::get_if<String>(&pattern);
    if (const auto* plural = std::get_if<PluralMessage>(&pattern)) {
        if (rule != IntegerPluralRule::EnglishOneOther && rule != IntegerPluralRule::OtherOnly) {
            fail(I18nErrorCode::UnsupportedPlural, key.id());
        }
        const auto count = std::get<std::uint64_t>(arg(plural->count.bytes()));
        const bool one = rule == IntegerPluralRule::EnglishOneOther && count == 1;
        if (one && !plural->one) {
            fail(I18nErrorCode::InvalidSchema, key.id());
        }
        text = one ? &*plural->one : &*plural->other;
    }
    std::string output;
    visit_template(
        *text, key.id(), [&](auto literal) { output.append(literal); },
        [&](auto name) { output.append(argument_text(arg(name))); });
    return string_from(output);
}

TranslationResult tryTranslate(const LocaleCatalog& catalog, const LocaleId& requested, const MessageKey& key,
                               const MessageArgs& args, std::span<const MessageCatalog> layers) {
    try {
        const auto found = lookupMessage(catalog, requested, key.id().view(), layers);
        if (const auto* entry = std::get_if<MessageTranslation>(&found)) {
            if (entry->key != key) {
                return I18nError{I18nErrorCode::InvalidSchema, requested, key.id()};
            }
            return formatMessage(key, entry->pattern, args, catalog.pack(entry->locale).data().plural);
        }
        return formatMessage(key, key.fallback(), args, IntegerPluralRule::EnglishOneOther);
    } catch (const I18nException& error) {
        auto diagnostic = error.error();
        diagnostic.locale = requested;
        return diagnostic;
    }
}

String translate(const LocaleCatalog& catalog, const LocaleId& requested, const MessageKey& key,
                 const MessageArgs& args, std::span<const MessageCatalog> layers) {
    auto result = tryTranslate(catalog, requested, key, args, layers);
    if (const auto* error = std::get_if<I18nError>(&result)) {
        throw I18nException(*error);
    }
    return std::get<String>(std::move(result));
}

} // namespace ryn
