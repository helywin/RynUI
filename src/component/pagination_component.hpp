#pragma once
#include "component/window_component_services.hpp"
#include <ryn/pagination.hpp>

namespace ryn::detail {
struct PaginationSnapshot final {
    std::size_t current{};
    std::size_t pages{};
    std::size_t page_size{};
    std::vector<runtime::ComponentId> items;
    std::vector<std::size_t> targets;
    bool hidden{};
};

class PaginationComponentHost final : public WindowComponentParticipant {
public:
    explicit PaginationComponentHost(WindowComponentServices&);
    ~PaginationComponentHost();
    void mount(const PaginationProps&);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    PaginationSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

private:
    void refresh(runtime::ComponentId, bool geometry);
    void request(runtime::ComponentId, std::size_t index);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
