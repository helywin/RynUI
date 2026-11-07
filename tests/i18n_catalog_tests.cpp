#include <ryn/i18n.hpp>

#include "support/i18n_test.hpp"

#include <array>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn_test::i18n;

void locale_contract() {
    const LocaleCatalog catalog;
    require(LocaleId("ZH-hans-cn").value() == "zh-Hans-CN", "locale normalization lost a subtag");
    require(catalog.resolve(LocaleId("en")).effective == locales::en_US, "explicit English alias missing");
    require(catalog.resolve(LocaleId("zh-Hans")).effective == locales::zh_CN, "explicit Hans alias missing");
    for (const auto id : {"zh-TW", "zh-Hant", "de-DE"}) {
        const auto result = catalog.resolve(LocaleId(id));
        require(result.requested == LocaleId(id) && result.effective == locales::en_US && result.diagnostic &&
                    result.diagnostic->code == I18nErrorCode::UnknownLocale,
                "unsupported locale was silently reported as supported");
    }
    for (const auto id : {"", "zh_XX", "en-", "a", "en--US", "\xFF"}) {
        rejects([&] { static_cast<void>(LocaleId(id)); }, I18nErrorCode::InvalidLocale);
    }
    const auto en = catalog.resolve(locales::en_US).pack;
    const auto zh = catalog.resolve(locales::zh_CN).pack;
    require(en.data().tour == TourLocale{} && zh.data().tour == TourLocale{u8"上一步", u8"下一步", u8"完成", u8"关闭"},
            "Tour built-in package is incomplete");
    for (const auto field : {&TourLocale::previous, &TourLocale::next, &TourLocale::finish, &TourLocale::close}) {
        auto invalid = std::make_unique<LocaleData>(zh.data());
        invalid->tour.*field = {};
        rejects([&] { static_cast<void>(LocalePack(*invalid)); }, I18nErrorCode::InvalidPack);
    }
    require(zh.data().picker.months[11] == String{u8"十二月"} &&
                zh.data().picker.confirm_range == String{u8"确认范围"} &&
                zh.data().transfer.pagination == zh.data().pagination &&
                zh.data().upload.image_preview == zh.data().image && zh.data().form.invalid == String{u8"字段值无效"} &&
                zh.data().statistic == en.data().statistic && zh.data().cascader.retry == String{u8"重试"} &&
                zh.data().color_picker.add_stop == String{u8"添加色标"},
            "complete built-in groups or compound locale data missing");
    require(en.data().picker.time_placeholder == String{u8"Select a time"} &&
                en.data().picker.date_time_placeholder == String{u8"Select date"} &&
                zh.data().picker.date_time_placeholder == String{u8"请选择日期和时间"} &&
                en.data().picker.year_placeholder == String{u8"Select year"} &&
                en.data().picker.quarter_placeholder == String{u8"Select quarter"} &&
                en.data().picker.month_placeholder == String{u8"Select month"} &&
                en.data().picker.week_placeholder == String{u8"Select week"} &&
                zh.data().picker.year_placeholder == String{u8"请选择年份"} &&
                zh.data().picker.quarter_placeholder == String{u8"请选择季度"} &&
                zh.data().picker.month_placeholder == String{u8"请选择月份"} &&
                zh.data().picker.week_placeholder == String{u8"请选择周"},
            "fixed Ant Design 6.6.5 picker placeholders or legacy time default missing");
    const std::array range_fields{&PickerLocale::year_range_placeholder, &PickerLocale::quarter_range_placeholder,
                                  &PickerLocale::month_range_placeholder, &PickerLocale::week_range_placeholder};
    const std::array english_ranges{
        std::array<String, 2>{u8"Start year", u8"End year"}, std::array<String, 2>{u8"Start quarter", u8"End quarter"},
        std::array<String, 2>{u8"Start month", u8"End month"}, std::array<String, 2>{u8"Start week", u8"End week"}};
    const std::array chinese_ranges{
        std::array<String, 2>{u8"开始年份", u8"结束年份"}, std::array<String, 2>{u8"开始季度", u8"结束季度"},
        std::array<String, 2>{u8"开始月份", u8"结束月份"}, std::array<String, 2>{u8"开始周", u8"结束周"}};
    for (std::size_t i = 0; i < range_fields.size(); ++i) {
        require(en.data().picker.*range_fields[i] == english_ranges[i] &&
                    zh.data().picker.*range_fields[i] == chinese_ranges[i],
                "picker range mode package values missing");
        for (std::size_t endpoint = 0; endpoint < 2; ++endpoint) {
            auto invalid = std::make_unique<LocaleData>(zh.data());
            (invalid->picker.*range_fields[i])[endpoint] = {};
            rejects([&] { static_cast<void>(LocalePack(*invalid)); }, I18nErrorCode::InvalidPack);
        }
    }
    for (const auto field : {&PickerLocale::year_placeholder, &PickerLocale::quarter_placeholder,
                             &PickerLocale::month_placeholder, &PickerLocale::week_placeholder}) {
        auto invalid = std::make_unique<LocaleData>(en.data());
        invalid->picker.*field = {};
        rejects([&] { static_cast<void>(LocalePack(*invalid)); }, I18nErrorCode::InvalidPack);
    }
    auto data = std::make_unique<LocaleData>(zh.data());
    const LocalePack frozen(*data);
    data->common.confirm = u8"mutated caller data";
    require(frozen == zh && frozen.data().common.confirm == String{u8"确定"},
            "immutable pack retained mutable aliases");
    data->common.confirm = {};
    rejects([&] { static_cast<void>(LocalePack(*data)); }, I18nErrorCode::InvalidPack);
    require(!String::from_utf8(std::string_view("\xC0\xAF", 2)), "invalid UTF-8 entered a public String");
    const LocaleId custom("fr-FR");
    const LocaleCatalog extended({{custom, frozen, locales::zh_CN}});
    const auto format = extended.resolveFormat(custom);
    require(format.requested == custom && format.effective == locales::zh_CN && format.diagnostic &&
                format.diagnostic->code == I18nErrorCode::UnsupportedFormat,
            "translation support hid its actual format fallback");
    require(extended.resolve(custom).candidates == std::vector<LocaleId>{custom, locales::zh_CN, locales::en_US},
            "explicit fallback chain changed order");
    require(extended == LocaleCatalog({{custom, LocalePack(frozen.data()), locales::zh_CN}}),
            "equal locale catalogs depended on allocation identity");
    rejects([&] { static_cast<void>(LocaleCatalog({{custom, frozen, LocaleId("xx")}})); },
            I18nErrorCode::UnknownLocale);
    rejects([&] { static_cast<void>(LocaleCatalog({{custom, frozen, custom}})); }, I18nErrorCode::InvalidFallback);
    rejects([&] { static_cast<void>(LocaleCatalog({{custom, frozen}, {custom, frozen}})); }, I18nErrorCode::Duplicate);
    rejects(
        [&] {
            static_cast<void>(LocaleCatalog({}, {{LocaleId("aa"), LocaleId("bb")}, {LocaleId("bb"), LocaleId("aa")}}));
        },
        I18nErrorCode::InvalidFallback);
    auto format_a = std::make_unique<LocaleData>(frozen.data());
    auto format_b = std::make_unique<LocaleData>(frozen.data());
    format_a->formats.source = LocaleId("de-DE");
    format_b->formats.source = custom;
    rejects(
        [&] {
            static_cast<void>(
                LocaleCatalog({{custom, LocalePack(*format_a)}, {LocaleId("de-DE"), LocalePack(*format_b)}}));
        },
        I18nErrorCode::InvalidFallback);
}

void messages_contract() {
    const LocaleCatalog catalog;
    const MessageKey greeting{
        u8"app.greeting", String{u8"Hello {name}: {{literal}}"}, {{u8"name", MessageArgType::Text}}};
    const MessageCatalog parent({{locales::zh_CN, greeting, String{u8"你好 {name}：{{原样}}"}},
                                 {locales::en_US, greeting, String{u8"Parent {name}"}}});
    const MessageCatalog child({{locales::zh_CN, greeting, String{u8"内层 {name}"}}});
    const std::array layers{child, parent};
    const MessageArgs args{{u8"name", String{u8"{name} 中文"}}};
    require(translate(catalog, locales::zh_CN, greeting, args, layers) == String{u8"内层 {name} 中文"},
            "parameter text was reparsed");
    require(translate(catalog, locales::en_US, greeting, args, layers) == String{u8"Parent {name} 中文"},
            "nested English lookup accidentally used a parent's Chinese translation");
    require(translate(catalog, locales::en_US, greeting, args) == String{u8"Hello {name} 中文: {literal}"},
            "declared fallback or escaped braces failed");
    const auto missing = lookupMessage(catalog, locales::en_US, String{u8"app.absent"}.view(), layers);
    require(std::get<I18nError>(missing).code == I18nErrorCode::MissingKey, "raw lookup leaked an undeclared key");
    const MessageKey empty{u8"app.empty", String{}};
    require(translate(catalog, locales::zh_CN, empty).empty(), "explicit empty fallback was treated as missing");
    const MessageKey empty_plural{
        u8"app.empty_plural", PluralMessage{u8"count", String{}, String{}}, {{u8"count", MessageArgType::UInt}}};
    require(translate(catalog, locales::en_US, empty_plural, {{u8"count", std::uint64_t{2}}}).empty(),
            "an explicit empty plural branch was treated as missing");
    rejects(
        [&] {
            static_cast<void>(MessageKey(u8"app.no_other", PluralMessage{u8"count", String{u8"one"}, std::nullopt},
                                         {{u8"count", MessageArgType::UInt}}));
        },
        I18nErrorCode::InvalidSchema);
    const MessageKey plural{u8"app.selected",
                            PluralMessage{u8"count", String{u8"{count} item"}, u8"{count} items"},
                            {{u8"count", MessageArgType::UInt}}};
    const std::array plural_layers{
        MessageCatalog({{locales::zh_CN, plural, PluralMessage{u8"count", std::nullopt, u8"{count} 项"}}})};
    for (const auto count :
         {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{2}, std::numeric_limits<std::uint64_t>::max()}) {
        const MessageArgs values{{u8"count", count}};
        const auto english = translate(catalog, locales::en_US, plural, values, plural_layers);
        const auto chinese = translate(catalog, locales::zh_CN, plural, values, plural_layers);
        require(english.bytes().ends_with(count == 1 ? " item" : " items") && chinese.bytes().ends_with(" 项"),
                "integer plural rules lost zero/one/other/uint64 precision");
        if (count == std::numeric_limits<std::uint64_t>::max()) {
            require(chinese == String{u8"18446744073709551615 项"}, "uint64 count passed through floating point");
        }
    }
    const MessageKey exact{
        u8"app.exact",
        String{u8"{d}/{i}/{u}"},
        {{u8"d", MessageArgType::Decimal}, {u8"i", MessageArgType::Int}, {u8"u", MessageArgType::UInt}}};
    require(translate(catalog, locales::en_US, exact,
                      {{u8"u", std::numeric_limits<std::uint64_t>::max()},
                       {u8"d", Decimal("12345678901234567890.123456789")},
                       {u8"i", std::numeric_limits<std::int64_t>::min()}}) ==
                String{u8"12345678901234567890.123456789/-9223372036854775808/18446744073709551615"},
            "typed exact values lost digits");
    require(parent == MessageCatalog({{locales::en_US, greeting, String{u8"Parent {name}"}},
                                      {locales::zh_CN, greeting, String{u8"你好 {name}：{{原样}}"}}}),
            "message catalog equality depended on insertion order");
    for (int repeat = 0; repeat < 2; ++repeat) {
        require(std::get<I18nError>(tryTranslate(catalog, locales::en_US, greeting)).code ==
                    I18nErrorCode::MissingArgument,
                "repeat missing args did not reject");
        rejects(
            [&] {
                static_cast<void>(translate(catalog, locales::en_US, greeting,
                                            {{u8"name", String{u8"private business body"}}, {u8"name", String{}}}));
            },
            I18nErrorCode::Duplicate);
        rejects(
            [&] { static_cast<void>(translate(catalog, locales::en_US, greeting, {{u8"name", std::uint64_t{1}}})); },
            I18nErrorCode::ArgumentType);
        rejects([&] { static_cast<void>(translate(catalog, locales::en_US, greeting, {{u8"extra", String{}}})); },
                I18nErrorCode::ExtraArgument);
        rejects([&] { static_cast<void>(MessageCatalog({{locales::zh_CN, greeting, String{u8"{name"}}})); },
                I18nErrorCode::MalformedTemplate);
        rejects([&] { static_cast<void>(MessageCatalog({{locales::zh_CN, greeting, String{u8"{other}"}}})); },
                I18nErrorCode::InvalidSchema);
    }
    require(translate(catalog, locales::en_US, greeting, args).bytes().starts_with("Hello"), "valid recovery failed");
    rejects(
        [&] {
            static_cast<void>(formatMessage(plural, plural.fallback(), {{u8"count", std::uint64_t{1}}},
                                            IntegerPluralRule::Unsupported));
        },
        I18nErrorCode::UnsupportedPlural);
    rejects(
        [&] {
            static_cast<void>(MessageKey(u8"bad.key", String{u8"{x}"},
                                         {{u8"x", MessageArgType::Text}, {u8"x", MessageArgType::Text}}));
        },
        I18nErrorCode::InvalidSchema);
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        locale_contract();
        messages_contract();
        std::cout << "i18n catalog: 2 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
