#pragma once

#include <ryn/breakpoints.hpp>
#include <ryn/component.hpp>
#include <ryn/flex.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace ryn {

class MasonryKey final {
public:
    MasonryKey() = default;

    MasonryKey(String value) : value_(std::move(value)) {}

    template <std::size_t Size> MasonryKey(const char8_t (&value)[Size]) : value_(String{value}) {}

    MasonryKey(int value) : value_(static_cast<std::int64_t>(value)) {}

    MasonryKey(std::int64_t value) : value_(value) {}

    [[nodiscard]] const std::variant<String, std::int64_t>& value() const noexcept {
        return value_;
    }

    [[nodiscard]] bool valid() const noexcept {
        const auto* text = std::get_if<String>(&value_);
        return !text || !text->empty();
    }

    friend bool operator==(const MasonryKey&, const MasonryKey&) = default;

private:
    std::variant<String, std::int64_t> value_{String{}};
};

struct MasonryContentSlot final {};

using MasonryContent = SlotContent<MasonryContentSlot>;

template <typename Data = std::monostate> class MasonryItem final {
public:
    MasonryItem(MasonryKey key, Data data = {})
        : key_(std::move(key)), data_(std::make_shared<const Data>(std::move(data))) {}

    MasonryItem& data(Data value) {
        data_ = std::make_shared<const Data>(std::move(value));
        return *this;
    }

    MasonryItem& height(std::optional<LogicalLength> value) {
        height_ = value;
        return *this;
    }

    MasonryItem& column(std::optional<int> value) {
        column_ = value;
        return *this;
    }

    MasonryItem& children(MasonryContent value) {
        children_ = std::make_shared<const MasonryContent>(std::move(value));
        return *this;
    }

    [[nodiscard]] const MasonryKey& key() const noexcept {
        return key_;
    }

    [[nodiscard]] const Data& data() const noexcept {
        return *data_;
    }

    [[nodiscard]] std::optional<LogicalLength> height() const noexcept {
        return height_;
    }

    [[nodiscard]] std::optional<int> column() const noexcept {
        return column_;
    }

    [[nodiscard]] const std::shared_ptr<const MasonryContent>& children() const noexcept {
        return children_;
    }

    friend bool operator==(const MasonryItem&, const MasonryItem&) = default;

private:
    MasonryKey key_;
    std::shared_ptr<const Data> data_;
    std::optional<LogicalLength> height_;
    std::optional<int> column_;
    std::shared_ptr<const MasonryContent> children_;
};

struct MasonryGutter final {
    Responsive<LogicalLength> horizontal{dp(0)};
    Responsive<LogicalLength> vertical{dp(0)};
    friend bool operator==(const MasonryGutter&, const MasonryGutter&) = default;
};

struct MasonryPlacement final {
    MasonryKey key;
    int column{};
    friend bool operator==(const MasonryPlacement&, const MasonryPlacement&) = default;
};

namespace detail {
struct MasonryPropBridge final {
    template <typename T> static T read(const Prop<T>& prop) {
        if (const auto* value = std::get_if<typename Prop<T>::StaticValue>(&prop.source_)) {
            if constexpr (Prop<T>::indirect_static_value) {
                return **value;
            } else {
                return *value;
            }
        }
        return std::get<Binding<T>>(prop.source_).get();
    }
};

using MasonryValue = std::shared_ptr<const void>;

struct MasonryRecord final {
    MasonryKey key;
    MasonryValue value;
    std::optional<LogicalLength> height;
    std::optional<int> column;
    std::shared_ptr<const MasonryContent> children;
    friend bool operator==(const MasonryRecord&, const MasonryRecord&) = default;
};

struct MasonryOptions final {
    Prop<Responsive<int>> columns{Responsive<int>{3}};
    Prop<MasonryGutter> gutter{MasonryGutter{}};
    Prop<Breakpoints> breakpoints{Breakpoints{}};
    Prop<FlexDirection> direction{FlexDirection::LeftToRight};
    Prop<bool> fresh{false};
    Prop<std::uint64_t> refresh{0};
    std::function<void(std::vector<MasonryPlacement>)> on_layout;
    LayoutStyle layout;
};

using MasonryRenderer = std::function<void(MasonryKey, Binding<MasonryValue>, Binding<std::size_t>, Binding<int>)>;
void mount_masonry(MasonryOptions options, Prop<std::vector<MasonryRecord>> items, MasonryRenderer render);
} // namespace detail

template <typename Data> class MasonryItemView final {
public:
    MasonryItemView(MasonryKey key, Binding<detail::MasonryValue> value, Binding<std::size_t> index,
                    Binding<int> column)
        : key_(std::move(key)), value_(std::move(value)), index_(std::move(index)), column_(std::move(column)) {}

    [[nodiscard]] const MasonryKey& key() const noexcept {
        return key_;
    }

    [[nodiscard]] Data value() const {
        return std::static_pointer_cast<const MasonryItem<Data>>(value_.get())->data();
    }

    [[nodiscard]] Prop<Data> data() const {
        return bind([view = *this] { return view.value(); });
    }

    [[nodiscard]] Prop<std::size_t> index() const {
        return index_;
    }

    [[nodiscard]] Prop<int> column() const {
        return column_;
    }

    [[nodiscard]] std::size_t index_value() const {
        return index_.get();
    }

    [[nodiscard]] int column_value() const {
        return column_.get();
    }

    [[nodiscard]] std::optional<LogicalLength> height() const {
        return std::static_pointer_cast<const MasonryItem<Data>>(value_.get())->height();
    }

private:
    MasonryKey key_;
    Binding<detail::MasonryValue> value_;
    Binding<std::size_t> index_;
    Binding<int> column_;
};

template <typename Data = std::monostate> class MasonryProps final {
public:
    MasonryProps& items(Prop<std::vector<MasonryItem<Data>>> value) {
        items_ = std::move(value);
        return *this;
    }

    MasonryProps& columns(Prop<Responsive<int>> value) {
        options_.columns = std::move(value);
        return *this;
    }

    MasonryProps& columns(int value) {
        return columns(Responsive<int>{value});
    }

    MasonryProps& gutter(Prop<MasonryGutter> value) {
        options_.gutter = std::move(value);
        return *this;
    }

    MasonryProps& gutter(LogicalLength value) {
        return gutter(value, value);
    }

    MasonryProps& gutter(LogicalLength horizontal, LogicalLength vertical) {
        return gutter(MasonryGutter{Responsive<LogicalLength>{horizontal}, Responsive<LogicalLength>{vertical}});
    }

    MasonryProps& breakpoints(Prop<Breakpoints> value) {
        options_.breakpoints = std::move(value);
        return *this;
    }

    MasonryProps& direction(Prop<FlexDirection> value) {
        options_.direction = std::move(value);
        return *this;
    }

    MasonryProps& fresh(Prop<bool> value = true) {
        options_.fresh = std::move(value);
        return *this;
    }

    MasonryProps& refresh(Prop<std::uint64_t> value) {
        options_.refresh = std::move(value);
        return *this;
    }

    MasonryProps& itemRender(std::function<void(MasonryItemView<Data>)> value) {
        render_ = std::move(value);
        return *this;
    }

    MasonryProps& onLayoutChange(std::function<void(std::vector<MasonryPlacement>)> value) {
        options_.on_layout = std::move(value);
        return *this;
    }

    MasonryProps& layout(LayoutStyle value) {
        options_.layout = std::move(value);
        return *this;
    }

    void mount() const {
        auto records = bind([source = items_] {
            std::vector<detail::MasonryRecord> result;
            for (const auto& item : detail::MasonryPropBridge::read(source)) {
                result.push_back({item.key(), std::make_shared<const MasonryItem<Data>>(item), item.height(),
                                  item.column(), item.children()});
            }
            return result;
        });
        detail::MasonryRenderer renderer;
        if (render_) {
            renderer = [render = render_](MasonryKey key, Binding<detail::MasonryValue> value,
                                          Binding<std::size_t> index, Binding<int> column) {
                render(MasonryItemView<Data>{std::move(key), std::move(value), std::move(index), std::move(column)});
            };
        }
        detail::mount_masonry(options_, std::move(records), std::move(renderer));
    }

private:
    Prop<std::vector<MasonryItem<Data>>> items_{std::vector<MasonryItem<Data>>{}};
    std::function<void(MasonryItemView<Data>)> render_;
    detail::MasonryOptions options_;
};

template <typename Data> void Masonry(MasonryProps<Data> props) {
    props.mount();
}

} // namespace ryn
