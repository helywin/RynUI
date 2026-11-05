#include <ryn/form_values.hpp>
#include <algorithm>
#include <cmath>

namespace ryn {
bool FormNamePath::startsWith(const FormNamePath& prefix) const {
    return segments_.size() >= prefix.segments_.size() &&
           std::equal(prefix.segments_.begin(), prefix.segments_.end(), segments_.begin());
}

const FormData* FormData::at(const FormNamePath& path) const {
    auto* node = this;
    for (const auto& segment : path.segments()) {
        if (const auto* key = std::get_if<String>(&segment.value())) {
            const auto* object = std::get_if<Object>(&node->value);
            if (!object) {
                return nullptr;
            }
            const auto it = std::ranges::find(*object, *key, &Object::value_type::first);
            if (it == object->end()) {
                return nullptr;
            }
            node = &it->second;
        } else {
            const auto* array = std::get_if<Array>(&node->value);
            const auto index = std::get<std::size_t>(segment.value());
            if (!array || index >= array->size()) {
                return nullptr;
            }
            node = &(*array)[index];
        }
    }
    return node;
}

void FormData::set(const FormNamePath& path, FormData data) {
    data.validate();
    auto next = *this;
    auto* node = &next;
    for (const auto& segment : path.segments()) {
        if (const auto* key = std::get_if<String>(&segment.value())) {
            if (std::holds_alternative<std::monostate>(node->value)) {
                node->value = Object{};
            }
            auto* object = std::get_if<Object>(&node->value);
            if (!object || key->empty()) {
                throw std::invalid_argument("Conflicting or empty Form object path");
            }
            auto it = std::ranges::find(*object, *key, &Object::value_type::first);
            if (it == object->end()) {
                object->emplace_back(*key, FormData{});
                it = std::prev(object->end());
            }
            node = &it->second;
        } else {
            if (std::holds_alternative<std::monostate>(node->value)) {
                node->value = Array{};
            }
            auto* array = std::get_if<Array>(&node->value);
            const auto index = std::get<std::size_t>(segment.value());
            if (!array || index > 100000) {
                throw std::invalid_argument("Conflicting or oversized Form array path");
            }
            if (array->size() <= index) {
                array->resize(index + 1);
            }
            node = &(*array)[index];
        }
    }
    *node = std::move(data);
    *this = std::move(next);
}

void FormData::validate() const {
    if (const auto* leaf = std::get_if<FormValue>(&value)) {
        if (const auto* number = std::get_if<std::optional<double>>(leaf);
            number && *number && !std::isfinite(**number)) {
            throw std::invalid_argument("Form numbers must be finite");
        }
    } else if (const auto* object = std::get_if<Object>(&value)) {
        for (std::size_t i = 0; i < object->size(); ++i) {
            if ((*object)[i].first.empty() || std::any_of(object->begin(), object->begin() + i, [&](const auto& pair) {
                    return pair.first == (*object)[i].first;
                })) {
                throw std::invalid_argument("Form object keys must be nonempty and unique");
            }
            (*object)[i].second.validate();
        }
    } else if (const auto* array = std::get_if<Array>(&value)) {
        for (const auto& item : *array) {
            item.validate();
        }
    }
}

FormData FormResult::structuredValues() const {
    FormData result = structure;
    for (const auto& field : values) {
        result.set(field.path.empty() ? FormNamePath{field.name} : field.path, FormData{field.value});
    }
    return result;
}
} // namespace ryn
