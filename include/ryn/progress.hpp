#pragma once
#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
namespace detail {
class FeedbackComponentHost;
}

enum class ProgressType { Line, Circle, Dashboard };
enum class ProgressStatus { Normal, Success, Exception };

class ProgressProps final {
public:
    ProgressProps& type(Prop<ProgressType> value) {
        type_ = std::move(value);
        return *this;
    }

    ProgressProps& percent(Prop<float> value) {
        percent_ = std::move(value);
        return *this;
    }

    ProgressProps& status(Prop<ProgressStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    ProgressProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    ProgressProps& showInfo(Prop<bool> value) {
        show_info_ = std::move(value);
        return *this;
    }

    ProgressProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FeedbackComponentHost;
    Prop<ProgressType> type_{ProgressType::Line};
    Prop<float> percent_{0};
    Prop<ProgressStatus> status_{ProgressStatus::Normal};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<bool> show_info_{true};
    LayoutStyle layout_;
};

void Progress(ProgressProps props = {});
} // namespace ryn
