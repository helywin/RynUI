#pragma once
#include <ryn/form_values.hpp>
#include <ryn/form_validation.hpp>
#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
namespace detail {
class FormComponentHost;
struct FormListState;
struct FormListRow;
struct FormListRefState;
} // namespace detail

class FormListField final {
public:
    [[nodiscard]] std::uint64_t key() const;
    [[nodiscard]] std::size_t index() const;
    [[nodiscard]] Prop<std::size_t> position() const;
    [[nodiscard]] FormNamePath path() const;

private:
    friend class detail::FormComponentHost;
    friend class FormListRef;
    std::shared_ptr<detail::FormListState> list_;
    std::shared_ptr<detail::FormListRow> row_;
};

class FormListRef final {
public:
    FormListRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] FormFieldMeta meta() const;
    [[nodiscard]] std::vector<FormListField> fields() const;
    [[nodiscard]] bool add(FormData value = {}, std::optional<std::size_t> index = {}) const;
    [[nodiscard]] bool remove(std::vector<std::size_t> indices) const;
    [[nodiscard]] bool move(std::size_t from, std::size_t to) const;

private:
    friend class detail::FormComponentHost;
    friend class FormListProps;
    std::shared_ptr<detail::FormListRefState> state_;
};

using FormListRule = std::function<std::vector<FormIssue>(const FormData::Array&, const FormValidationContext&)>;

class FormListProps final {
public:
    FormListProps& rules(std::vector<FormListRule> value) {
        rules_ = std::move(value);
        return *this;
    }

    FormListProps& asyncValidator(FormAsyncValidator<FormData::Array> provider, OwnerThreadPost post) {
        if (!provider || !post) {
            throw std::invalid_argument("FormList async validation requires a provider and owner-thread post");
        }
        async_ = [provider = std::move(provider), post = std::move(post)](FormData::Array value,
                                                                          FormValidationContext context) {
            return AsyncOptionsSource<FormIssue>{
                [provider, value = std::move(value), context = std::move(context)](AsyncOptionsRequest request,
                                                                                   auto complete) mutable {
                    context.generation = request.generation;
                    context.cancelled = request.cancelled;
                    return provider(value, context, [complete = std::move(complete)](std::vector<FormIssue> issues) {
                        complete({std::move(issues), {}});
                    });
                },
                post};
        };
        return *this;
    }

    FormListProps& name(FormNamePath value) {
        name_ = std::move(value);
        return *this;
    }

    FormListProps& ref(const FormListRef& value) {
        reference_ = value.state_;
        return *this;
    }

    FormListProps& initialValue(FormData::Array value) {
        initial_ = std::move(value);
        return *this;
    }

    FormListProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    std::vector<FormListRule> rules_;
    std::function<AsyncOptionsSource<FormIssue>(FormData::Array, FormValidationContext)> async_;
    FormNamePath name_;
    FormData::Array initial_;
    std::shared_ptr<detail::FormListRefState> reference_;
    LayoutStyle layout_;
};

struct FormListItemSlot final {};

// Called once per stable row. Each row can contain rich controls and nested FormList.
using FormListItem = std::function<void(FormListField)>;
void FormList(FormListProps props, FormListItem item);
} // namespace ryn
