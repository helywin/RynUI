#pragma once

#include "component/window_component_services.hpp"

namespace ryn::detail {

struct TypographyInteractionSnapshot final {
  bool expanded{}, truncated{}, copy_available{}, copied{}, copy_failed{},
      editing{}, edit_pending{};
  String original, displayed, draft;
};

class TypographyComponentHost final : public WindowComponentParticipant {
public:
  explicit TypographyComponentHost(WindowComponentServices &services);
  ~TypographyComponentHost();
  bool mount(const TypographyProps &, TypographySemantics::Role,
             const Prop<TypographyLevel> &);
  void mount_link(const LinkProps &);
  [[nodiscard]] TypographyInteractionSnapshot
      snapshot(runtime::ComponentId) const;
  [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
    return mounted_;
  }
  void *begin_mount() noexcept override;
  void end_mount(void *) noexcept override;
  void on_destroy() noexcept override;
  void on_dispose() noexcept override {
    mounted_.clear();
    actions_.clear();
  }
  void on_clipboard_bound() override;
  void on_window_active(bool) override;
  void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
  std::size_t tick_auxiliary(animation::AnimationTime) override;
  std::optional<animation::AnimationTime>
  next_auxiliary_deadline() const override;

private:
  void refresh(runtime::ComponentId);
  void copy(runtime::ComponentId);
  void edit(runtime::ComponentId);
  void commit(runtime::ComponentId, String, bool focus_back);
  void cancel(runtime::ComponentId);
  WindowComponentServices *services_;
  std::vector<runtime::ComponentId> mounted_;
  std::vector<runtime::ComponentId> actions_;
};

bool try_mount_typography_interactions(const TypographyProps &,
                                       TypographySemantics::Role,
                                       const Prop<TypographyLevel> &);

} // namespace ryn::detail
