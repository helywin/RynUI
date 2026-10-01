#pragma once
#include "component/window_component_services.hpp"
#include <ryn/divider.hpp>

namespace ryn::detail {
struct DividerSnapshot {
  runtime::Rect left, right, label;
  float margin{}, line_width{};
  bool vertical{}, has_label{}, dashed{}, plain{};
};
class DividerComponentHost final : public WindowComponentParticipant {
public:
  explicit DividerComponentHost(WindowComponentServices &);
  ~DividerComponentHost();
  void mount(const DividerProps &, const std::optional<DividerText> &);
  [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
    return mounted_;
  }
  [[nodiscard]] DividerSnapshot snapshot(runtime::ComponentId) const;
  void *begin_mount() noexcept override;
  void end_mount(void *) noexcept override;
  void on_destroy() noexcept override;
  void on_dispose() noexcept override { mounted_.clear(); }
  void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
  void update(runtime::ComponentId, bool geometry);
  WindowComponentServices *services_;
  std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
