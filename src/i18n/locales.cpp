#include <ryn/i18n.hpp>

#include <algorithm>
#include <set>

namespace ryn {
namespace {

[[noreturn]] void invalid(I18nErrorCode code, const LocaleId& id = {}) {
    throw I18nException({code, id, {}});
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

char upper(char c) {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - ('a' - 'A')) : c;
}

bool alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void require_strings(std::initializer_list<const String*> values) {
    for (const auto* value : values) {
        if (value->empty()) {
            invalid(I18nErrorCode::InvalidPack);
        }
    }
}

template <std::size_t N> void require_array(const std::array<String, N>& values) {
    for (const auto& value : values) {
        require_strings({&value});
    }
}

void validate_image(const ImageLocale& p) {
    require_strings({&p.loading, &p.error, &p.preview, &p.zoom_in, &p.zoom_out, &p.rotate_left, &p.rotate_right,
                     &p.flip_x, &p.flip_y, &p.reset, &p.previous, &p.next, &p.close, &p.count});
    const MessageKey count{u8"ryn.image.count",
                           String{u8"{current} / {total}"},
                           {{u8"current", MessageArgType::UInt}, {u8"total", MessageArgType::UInt}}};
    static_cast<void>(MessageCatalog({{locales::en_US, count, p.count}}));
}

void validate_pagination(const PaginationLocale& p) {
    require_strings({&p.items_per_page, &p.page_separator, &p.jump_placeholder, &p.total});
    const MessageKey total{u8"ryn.pagination.total", String{u8"{total} items"}, {{u8"total", MessageArgType::UInt}}};
    static_cast<void>(MessageCatalog({{locales::en_US, total, p.total}}));
}

void validate(const LocaleData& d) {
    require_strings({&d.tour.previous, &d.tour.next, &d.tour.finish, &d.tour.close});
    try {
        d.picker.validate();
    } catch (const std::invalid_argument&) {
        invalid(I18nErrorCode::InvalidPack);
    }
    require_strings(
        {&d.common.confirm, &d.common.cancel, &d.common.empty, &d.common.loading, &d.common.retry, &d.common.close});
    const auto& p = d.picker;
    require_array(p.months);
    require_array(p.short_months);
    require_array(p.weekdays);
    require_array(p.range_placeholder);
    require_array(p.time_range_placeholder);
    require_array(p.year_range_placeholder);
    require_array(p.quarter_range_placeholder);
    require_array(p.month_range_placeholder);
    require_array(p.week_range_placeholder);
    require_strings({&p.am,
                     &p.pm,
                     &p.now,
                     &p.today,
                     &p.confirm,
                     &p.cancel,
                     &p.date_placeholder,
                     &p.time_placeholder,
                     &p.date_time_placeholder,
                     &p.clear,
                     &p.clear_range,
                     &p.confirm_range,
                     &p.cancel_range,
                     &p.month,
                     &p.year,
                     &p.date_format,
                     &p.time_format,
                     &p.date_time_format,
                     &p.month_panel_format,
                     &p.year_placeholder,
                     &p.quarter_placeholder,
                     &p.month_placeholder,
                     &p.week_placeholder});
    validate_pagination(d.pagination);
    const auto& t = d.transfer;
    require_array(t.titles);
    require_array(t.empty);
    require_array(t.actions);
    require_strings({&t.search_placeholder, &t.item_unit, &t.items_unit, &t.loading, &t.select_all, &t.deselect_all,
                     &t.select_current, &t.select_invert, &t.clear, &t.remove, &t.remove_all, &t.remove_current,
                     &t.count_one, &t.count_other});
    const MessageKey count{u8"ryn.transfer.count",
                           String{u8"{selected}/{total} items"},
                           {{u8"selected", MessageArgType::UInt}, {u8"total", MessageArgType::UInt}}};
    static_cast<void>(MessageCatalog({{locales::en_US, count, t.count_one}, {locales::zh_CN, count, t.count_other}}));
    validate_pagination(t.pagination);
    const auto& u = d.upload;
    require_strings({&u.select, &u.drop, &u.ready, &u.uploading, &u.done, &u.error, &u.cancelled, &u.preview,
                     &u.download, &u.remove, &u.retry, &u.cancel});
    validate_image(u.image_preview);
    validate_image(d.image);
    require_strings({&d.form.optional,      &d.form.validating,         &d.form.required,
                     &d.form.invalid,       &d.descriptions.separator,  &d.qrcode.expired,
                     &d.qrcode.refresh,     &d.qrcode.loading,          &d.qrcode.scanned,
                     &d.table.empty,        &d.table.loading,           &d.cascader.empty,
                     &d.cascader.loading,   &d.cascader.loading_failed, &d.cascader.search_paths,
                     &d.cascader.retry,     &d.color_picker.single,     &d.color_picker.gradient,
                     &d.color_picker.clear, &d.color_picker.add_stop,   &d.color_picker.remove,
                     &d.color_picker.color});
    const auto& s = d.statistic;
    require_strings({&s.decimal_separator});
    require_array(s.digits);
    if (s.primary_group < 1 || s.primary_group > 9 || s.secondary_group < 1 || s.secondary_group > 9 ||
        s.decimal_separator == s.group_separator ||
        static_cast<unsigned>(d.plural) > static_cast<unsigned>(IntegerPluralRule::Unsupported)) {
        invalid(I18nErrorCode::InvalidPack);
    }
    for (std::size_t i = 0; i < s.digits.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (s.digits[i] == s.digits[j]) {
                invalid(I18nErrorCode::InvalidPack);
            }
        }
    }
}

LocalePack english() {
    static const LocalePack pack = [] {
        auto data = std::make_unique<LocaleData>();
        data->plural = IntegerPluralRule::EnglishOneOther;
        return LocalePack(std::move(*data));
    }();
    return pack;
}

LocalePack make_chinese() {
    auto d = std::make_unique<LocaleData>();
    d->common = {u8"确定", u8"取消", u8"暂无数据", u8"加载中…", u8"重试", u8"关闭"};
    auto& p = d->picker;
    p.months = {u8"一月", u8"二月", u8"三月", u8"四月", u8"五月",   u8"六月",
                u8"七月", u8"八月", u8"九月", u8"十月", u8"十一月", u8"十二月"};
    p.short_months = {u8"1月", u8"2月", u8"3月", u8"4月",  u8"5月",  u8"6月",
                      u8"7月", u8"8月", u8"9月", u8"10月", u8"11月", u8"12月"};
    p.weekdays = {u8"日", u8"一", u8"二", u8"三", u8"四", u8"五", u8"六"};
    p.am = u8"上午";
    p.pm = u8"下午";
    p.now = u8"此刻";
    p.today = u8"今天";
    p.confirm = u8"确定";
    p.cancel = u8"取消";
    p.first_day = 1;
    p.first_week_contains = 4;
    p.date_placeholder = u8"请选择日期";
    p.time_placeholder = u8"请选择时间";
    p.date_time_placeholder = u8"请选择日期和时间";
    p.range_placeholder = {u8"开始日期", u8"结束日期"};
    p.time_range_placeholder = {u8"开始时间", u8"结束时间"};
    p.clear = u8"清除";
    p.clear_range = u8"清除范围";
    p.confirm_range = u8"确认范围";
    p.cancel_range = u8"取消范围";
    p.month = u8"月";
    p.year = u8"年";
    p.date_format = u8"YYYY-MM-DD";
    p.time_format = u8"HH:mm:ss";
    p.date_time_format = u8"YYYY-MM-DD HH:mm:ss";
    p.month_panel_format = u8"YYYY[年]M[月]";
    p.year_placeholder = u8"请选择年份";
    p.quarter_placeholder = u8"请选择季度";
    p.month_placeholder = u8"请选择月份";
    p.week_placeholder = u8"请选择周";
    p.year_range_placeholder = {u8"开始年份", u8"结束年份"};
    p.quarter_range_placeholder = {u8"开始季度", u8"结束季度"};
    p.month_range_placeholder = {u8"开始月份", u8"结束月份"};
    p.week_range_placeholder = {u8"开始周", u8"结束周"};
    d->pagination = {u8" 条/页", u8"/ ", u8"跳至", u8"共 {total} 条"};
    auto& t = d->transfer;
    t.titles = {u8"源列表", u8"目标列表"};
    t.empty = {u8"暂无数据", u8"暂无数据"};
    t.search_placeholder = u8"请输入搜索内容";
    t.item_unit = u8"项";
    t.items_unit = u8"项";
    t.loading = u8"加载中…";
    t.select_all = u8"全选";
    t.deselect_all = u8"取消全选";
    t.select_current = u8"选择当前页";
    t.select_invert = u8"反选";
    t.clear = u8"清空选择";
    t.remove = u8"移除";
    t.remove_all = u8"全部移除";
    t.remove_current = u8"移除当前页";
    t.actions = {u8"移至目标列表", u8"移至源列表"};
    t.pagination = d->pagination;
    t.count_one = u8"已选 {selected} 项，共 {total} 项";
    t.count_other = t.count_one;
    d->image = {u8"图片加载中", u8"图片不可用",
                u8"预览",       u8"放大",
                u8"缩小",       u8"向左旋转",
                u8"向右旋转",   u8"水平翻转",
                u8"垂直翻转",   u8"重置",
                u8"上一张",     u8"下一张",
                u8"关闭预览",   u8"第 {current} 张，共 {total} 张"};
    d->upload = {u8"选择文件", u8"点击或拖拽文件到此处",
                 u8"就绪",     u8"上传中",
                 u8"已完成",   u8"上传失败",
                 u8"已取消",   u8"预览",
                 u8"下载",     u8"移除",
                 u8"重试",     u8"取消",
                 d->image};
    d->form = {u8"（选填）", u8"校验中", u8"此项为必填项", u8"字段值无效"};
    d->descriptions = {u8"："};
    d->qrcode = {u8"二维码已过期", u8"刷新", u8"加载中", u8"已扫描"};
    d->table = {u8"暂无数据", u8"加载中…"};
    d->tour = {u8"上一步", u8"下一步", u8"完成", u8"关闭"};
    d->cascader = {u8"暂无数据", u8"加载中…", u8"加载失败", u8"搜索路径", u8"重试"};
    d->color_picker = {u8"单色", u8"渐变", u8"清除", u8"添加色标", u8"移除", u8"颜色"};
    d->plural = IntegerPluralRule::OtherOnly;
    d->formats.source = locales::zh_CN;
    return LocalePack(std::move(*d));
}

LocalePack chinese() {
    static const LocalePack pack = make_chinese();
    return pack;
}

} // namespace

// Pure data validation moved unchanged from component/picker_format.cpp.
void PickerLocale::validate() const {
    if (first_day > 6 || first_week_contains < 1 || first_week_contains > 7 || year_offset < -9998 ||
        year_offset > 9998 || am.empty() || pm.empty() || am == pm ||
        std::ranges::any_of(months, [](const auto& s) { return s.empty(); }) ||
        std::ranges::any_of(short_months, [](const auto& s) { return s.empty(); }) ||
        std::ranges::any_of(weekdays, [](const auto& s) { return s.empty(); })) {
        throw std::invalid_argument("Invalid Gregorian picker locale");
    }
    for (const auto* names : {&months, &short_months}) {
        std::set<std::string> unique;
        for (const auto& name : *names) {
            if (!unique.insert(std::string{name.bytes()}).second) {
                throw std::invalid_argument("Picker locale month names must be unambiguous");
            }
        }
    }
}

LocaleId::LocaleId() : value_("en-US") {}

LocaleId::LocaleId(std::string_view value) {
    if (value.empty() || value.size() > 63) {
        invalid(I18nErrorCode::InvalidLocale);
    }
    std::size_t begin = 0;
    unsigned part = 0;
    while (begin < value.size()) {
        const auto separator = value.find('-', begin);
        const auto end = separator == value.npos ? value.size() : separator;
        const auto length = end - begin;
        if (!length || length > 8 || (!part && length < 2)) {
            invalid(I18nErrorCode::InvalidLocale);
        }
        bool letters = true;
        for (std::size_t i = begin; i < end; ++i) {
            const auto c = value[i];
            if (!alpha(c) && (!part || c < '0' || c > '9')) {
                invalid(I18nErrorCode::InvalidLocale);
            }
            letters = letters && alpha(c);
        }
        if (part) {
            value_.push_back('-');
        }
        for (std::size_t i = begin; i < end; ++i) {
            char c = lower(value[i]);
            if (part && letters && (length == 2 || (length == 4 && i == begin))) {
                c = upper(c);
            }
            value_.push_back(c);
        }
        ++part;
        if (separator == value.npos) {
            break;
        }
        begin = end + 1;
        if (begin == value.size()) {
            invalid(I18nErrorCode::InvalidLocale);
        }
    }
}

const std::string& LocaleId::value() const noexcept {
    return value_;
}

LocalePack::LocalePack(LocaleData data) {
    validate(data);
    data_ = std::make_shared<const LocaleData>(std::move(data));
}

const LocaleData& LocalePack::data() const noexcept {
    return *data_;
}

bool operator==(const LocalePack& left, const LocalePack& right) {
    return left.data_ == right.data_ || *left.data_ == *right.data_;
}

struct LocaleCatalog::Data final {
    std::vector<LocaleDefinition> definitions;
    std::vector<LocaleAlias> aliases;
    LocaleId fallback;
    friend bool operator==(const Data&, const Data&) = default;

    const LocaleDefinition* definition(const LocaleId& id) const {
        const auto it = std::find_if(definitions.begin(), definitions.end(), [&](const auto& d) { return d.id == id; });
        return it == definitions.end() ? nullptr : &*it;
    }

    LocaleId canonical(LocaleId id) const {
        std::set<LocaleId> visited;
        for (;;) {
            if (!visited.emplace(id).second) {
                invalid(I18nErrorCode::InvalidFallback, id);
            }
            if (definition(id)) {
                return id;
            }
            const auto it = std::find_if(aliases.begin(), aliases.end(), [&](const auto& a) { return a.id == id; });
            if (it == aliases.end()) {
                invalid(I18nErrorCode::UnknownLocale, id);
            }
            id = it->target;
        }
    }
};

LocaleCatalog::LocaleCatalog() : data_(builtin().data_) {}

LocaleCatalog::LocaleCatalog(std::vector<LocaleDefinition> definitions, std::vector<LocaleAlias> aliases,
                             LocaleId fallback) {
    definitions.push_back({locales::en_US, english(), std::nullopt});
    definitions.push_back({locales::zh_CN, chinese(), locales::en_US});
    aliases.push_back({LocaleId("en"), locales::en_US});
    aliases.push_back({LocaleId("zh"), locales::zh_CN});
    aliases.push_back({LocaleId("zh-Hans"), locales::zh_CN});
    std::sort(definitions.begin(), definitions.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(aliases.begin(), aliases.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    auto data = std::make_shared<Data>(Data{std::move(definitions), std::move(aliases), std::move(fallback)});
    std::set<LocaleId> identities;
    for (const auto& d : data->definitions) {
        if (!identities.emplace(d.id).second) {
            invalid(I18nErrorCode::Duplicate, d.id);
        }
    }
    for (const auto& a : data->aliases) {
        if (!identities.emplace(a.id).second) {
            invalid(I18nErrorCode::Duplicate, a.id);
        }
        static_cast<void>(data->canonical(a.id));
    }
    static_cast<void>(data->canonical(data->fallback));
    for (const auto& d : data->definitions) {
        std::set<LocaleId> visited;
        const auto* current = &d;
        for (;;) {
            if (!visited.emplace(current->id).second) {
                invalid(I18nErrorCode::InvalidFallback, current->id);
            }
            if (!current->fallback) {
                if (current->id != locales::en_US) {
                    invalid(I18nErrorCode::InvalidFallback, current->id);
                }
                break;
            }
            current = data->definition(data->canonical(*current->fallback));
        }
        std::set<LocaleId> format_visited;
        current = &d;
        for (;;) {
            if (!format_visited.emplace(current->id).second) {
                invalid(I18nErrorCode::InvalidFallback, current->id);
            }
            const auto source = data->canonical(current->pack.data().formats.source);
            if (source == current->id) {
                if (!current->pack.data().formats.decimal || !current->pack.data().formats.gregorian) {
                    invalid(I18nErrorCode::InvalidPack, current->id);
                }
                break;
            }
            current = data->definition(source);
        }
    }
    data_ = std::move(data);
}

const LocaleCatalog& LocaleCatalog::builtin() {
    static const LocaleCatalog catalog(std::vector<LocaleDefinition>{});
    return catalog;
}

LocaleResolution LocaleCatalog::resolve(const LocaleId& requested) const {
    LocaleId effective;
    std::optional<I18nError> diagnostic;
    try {
        effective = data_->canonical(requested);
    } catch (const I18nException&) {
        effective = data_->canonical(data_->fallback);
        diagnostic = I18nError{I18nErrorCode::UnknownLocale, requested, {}};
    }
    const auto* definition = data_->definition(effective);
    LocaleResolution result{requested, effective, definition->pack, {}, std::move(diagnostic)};
    for (;;) {
        result.candidates.push_back(definition->id);
        if (!definition->fallback) {
            break;
        }
        definition = data_->definition(data_->canonical(*definition->fallback));
    }
    return result;
}

const LocalePack& LocaleCatalog::pack(const LocaleId& exact_id) const {
    if (const auto* definition = data_->definition(exact_id)) {
        return definition->pack;
    }
    invalid(I18nErrorCode::UnknownLocale, exact_id);
}

LocaleResolution LocaleCatalog::resolveFormat(const LocaleId& requested) const {
    auto result = resolve(requested);
    for (;;) {
        const auto source = data_->canonical(result.pack.data().formats.source);
        if (source == result.effective) {
            return result;
        }
        result = resolve(source);
        result.requested = requested;
        result.diagnostic = I18nError{I18nErrorCode::UnsupportedFormat, requested, {}};
    }
}

bool operator==(const LocaleCatalog& left, const LocaleCatalog& right) {
    return left.data_ == right.data_ || *left.data_ == *right.data_;
}

} // namespace ryn
