#pragma once
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace ryn {
enum class ListyDirection { Vertical, Horizontal };
enum class ListyScrollAlign { Start, End, Auto };

struct ListyVirtualization final {
    float estimated_item_height{48.0F};
    float estimated_group_height{32.0F};
    std::size_t overscan{2};
    friend bool operator==(const ListyVirtualization&, const ListyVirtualization&) = default;
};

struct ListyContentSlot final {};

struct ListyEmptySlot final {};

using ListyContent = SlotContent<ListyContentSlot>;
using ListyEmpty = SlotContent<ListyEmptySlot>;

namespace detail {
class ListyComponentHost;
struct ListyRefState;

struct ListyPropBridge final {
    template <class T> static T read(const Prop<T>& prop) {
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
} // namespace detail

class ListyRef final {
public:
    ListyRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool scrollTo(float offset) const;
    [[nodiscard]] bool scrollTo(String key, ListyScrollAlign align = ListyScrollAlign::Auto, float offset = 0) const;
    [[nodiscard]] bool scrollToGroup(String key, ListyScrollAlign align = ListyScrollAlign::Auto,
                                     float offset = 0) const;

private:
    friend class detail::ListyComponentHost;
    std::shared_ptr<detail::ListyRefState> state_;
};

template <class Data = String> class ListyItem final {
public:
    ListyItem(String key, Data data) : key_(std::move(key)), data_(std::make_shared<const Data>(std::move(data))) {}

    ListyItem& data(Data value) {
        data_ = std::make_shared<const Data>(std::move(value));
        return *this;
    }

    [[nodiscard]] const String& key() const noexcept {
        return key_;
    }

    [[nodiscard]] const std::shared_ptr<const Data>& data() const noexcept {
        return data_;
    }

    friend bool operator==(const ListyItem&, const ListyItem&) = default;

private:
    String key_;
    std::shared_ptr<const Data> data_;
};

namespace detail {
struct ListyRecord final {
    String key;
    std::shared_ptr<const void> value;
    String group;
    friend bool operator==(const ListyRecord&, const ListyRecord&) = default;
};

struct ListyOptions final {
    Prop<ListyDirection> direction{ListyDirection::Vertical};
    Prop<std::size_t> columns{1};
    Prop<bool> rtl{false};
    Prop<bool> disabled{false};
    Prop<bool> visible{true};
    Prop<bool> pagination{false};
    std::optional<Prop<std::size_t>> current;
    std::size_t default_current{1};
    Prop<std::size_t> page_size{10};
    std::optional<Prop<float>> height;
    std::optional<ListyRef> ref;
    std::function<void(std::size_t, std::size_t)> on_change;
    std::function<void(float)> on_scroll;
    std::optional<ListyContent> header;
    std::optional<ListyContent> footer;
    std::optional<ListyEmpty> empty;
    LayoutStyle layout;
    bool explicit_rtl{};
    bool explicit_disabled{};
    Prop<bool> virtualized{false};
    Prop<bool> sticky{false};
    ListyVirtualization virtualization;
    std::function<void(String, Binding<std::vector<std::shared_ptr<const void>>>)> group_render;
};

using ListyRenderer = std::function<void(String, Binding<std::shared_ptr<const void>>, Binding<std::size_t>)>;
void mount_listy(ListyOptions, Prop<std::vector<ListyRecord>>, ListyRenderer);
} // namespace detail

// The renderer runs once for each stable key. Bind data()/index() inside its retained controls.
template <class Data> struct ListyItemView final {
    String key;
    Binding<std::shared_ptr<const void>> source;
    Binding<std::size_t> position;

    [[nodiscard]] Prop<Data> data() const {
        return bind([value = source] { return *std::static_pointer_cast<const Data>(value.get()); });
    }

    [[nodiscard]] Prop<std::size_t> index() const {
        return position;
    }
};

template <class Data> struct ListyGroupView final {
    String key;
    Binding<std::vector<std::shared_ptr<const void>>> source;

    [[nodiscard]] Prop<std::vector<Data>> items() const {
        return bind([value = source] {
            std::vector<Data> result;
            for (const auto& item : value.get()) {
                result.push_back(*std::static_pointer_cast<const Data>(item));
            }
            return result;
        });
    }
};

template <class Data> struct ListyGroup final {
    std::function<String(const Data&)> key;
    std::function<void(ListyGroupView<Data>)> title;
};

template <class Data = String> class ListyProps final {
public:
    ListyProps& items(Prop<std::vector<ListyItem<Data>>> value) {
        items_ = std::move(value);
        return *this;
    }

    ListyProps& itemRender(std::function<void(ListyItemView<Data>)> value) {
        render_ = std::move(value);
        return *this;
    }

    ListyProps& group(ListyGroup<Data> value) {
        group_ = std::move(value);
        return *this;
    }

    ListyProps& virtualized(Prop<bool> value = true) {
        options_.virtualized = std::move(value);
        return *this;
    }

    ListyProps& virtualization(ListyVirtualization value) {
        options_.virtualization = value;
        return *this;
    }

    ListyProps& sticky(Prop<bool> value = true) {
        options_.sticky = std::move(value);
        return *this;
    }

    ListyProps& direction(Prop<ListyDirection> value) {
        options_.direction = std::move(value);
        return *this;
    }

    ListyProps& columns(Prop<std::size_t> value) {
        options_.columns = std::move(value);
        return *this;
    }

    ListyProps& rtl(Prop<bool> value = true) {
        options_.rtl = std::move(value);
        options_.explicit_rtl = true;
        return *this;
    }

    ListyProps& disabled(Prop<bool> value = true) {
        options_.disabled = std::move(value);
        options_.explicit_disabled = true;
        return *this;
    }

    ListyProps& visible(Prop<bool> value = true) {
        options_.visible = std::move(value);
        return *this;
    }

    ListyProps& pagination(Prop<bool> value = true) {
        options_.pagination = std::move(value);
        return *this;
    }

    ListyProps& current(Prop<std::size_t> value) {
        options_.current = std::move(value);
        return *this;
    }

    ListyProps& defaultCurrent(std::size_t value) {
        options_.default_current = value;
        return *this;
    }

    ListyProps& pageSize(Prop<std::size_t> value) {
        options_.page_size = std::move(value);
        return *this;
    }

    ListyProps& height(Prop<float> value) {
        options_.height = std::move(value);
        return *this;
    }

    ListyProps& ref(ListyRef value) {
        options_.ref = std::move(value);
        return *this;
    }

    ListyProps& onChange(std::function<void(std::size_t, std::size_t)> value) {
        options_.on_change = std::move(value);
        return *this;
    }

    ListyProps& onScroll(std::function<void(float)> value) {
        options_.on_scroll = std::move(value);
        return *this;
    }

    ListyProps& header(ListyContent value) {
        options_.header = std::move(value);
        return *this;
    }

    ListyProps& footer(ListyContent value) {
        options_.footer = std::move(value);
        return *this;
    }

    ListyProps& emptyContent(ListyEmpty value) {
        options_.empty = std::move(value);
        return *this;
    }

    ListyProps& layout(LayoutStyle value) {
        options_.layout = std::move(value);
        return *this;
    }

    void mount() const {
        if (group_ && (!group_->key || !group_->title)) {
            throw std::invalid_argument("Listy grouping requires key and title factories");
        }
        auto records = bind([source = items_, group = group_] {
            std::vector<detail::ListyRecord> result;
            for (const auto& item : detail::ListyPropBridge::read(source)) {
                result.push_back({item.key(), item.data(), group ? group->key(*item.data()) : String{}});
            }
            return result;
        });
        detail::ListyRenderer renderer;
        if (render_) {
            renderer = [render = render_](auto key, auto value, auto index) {
                render(ListyItemView<Data>{std::move(key), std::move(value), std::move(index)});
            };
        }
        auto options = options_;
        if (group_) {
            options.group_render = [render = group_->title](auto key, auto values) {
                render(ListyGroupView<Data>{std::move(key), std::move(values)});
            };
        }
        detail::mount_listy(std::move(options), std::move(records), std::move(renderer));
    }

private:
    Prop<std::vector<ListyItem<Data>>> items_{std::vector<ListyItem<Data>>{}};
    std::function<void(ListyItemView<Data>)> render_;
    detail::ListyOptions options_;
    std::optional<ListyGroup<Data>> group_;
};

template <class Data> void Listy(ListyProps<Data> props) {
    props.mount();
}
} // namespace ryn
