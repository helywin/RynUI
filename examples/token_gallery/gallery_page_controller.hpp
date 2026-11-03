#pragma once

#include "gallery_page_model.hpp"
#include "gallery_page_state.hpp"
#include "reference_surface.hpp"

#include <ryn/component.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rynui::example {

struct GalleryPageTicketState final {
    std::uint64_t generation{1};
    bool alive{true};
    bool accepting{};
};

class GalleryPageTicket final {
public:
    [[nodiscard]] bool valid() const noexcept;

private:
    friend class GalleryPageController;
    std::weak_ptr<GalleryPageTicketState> state_;
    std::uint64_t generation_{};
};

struct GalleryPageContent final {
    ryn::Content content;
    std::function<void(GalleryPageDraft&)> save;
    std::function<void()> before_retire;
};

class GalleryPageController final {
public:
    using Factory = std::function<GalleryPageContent(const GalleryPageRoute&, const GalleryPageDraft&)>;

    GalleryPageController(ReferenceSurfaceHost& surfaces, GalleryPageModel& model,
                          ryn::runtime::FrameRequestState& frames, ryn::runtime::ComponentId parent, Factory factory);
    GalleryPageController(const GalleryPageController&) = delete;
    GalleryPageController& operator=(const GalleryPageController&) = delete;
    ~GalleryPageController();

    // Call process() only after an enclosing input callback/dispatch has returned.
    bool request(GalleryPageRoute route);
    bool process();
    bool back();
    bool forward();
    [[nodiscard]] bool can_back() const noexcept;
    [[nodiscard]] bool can_forward() const noexcept;
    [[nodiscard]] bool pending() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] const std::string& last_error() const noexcept;
    [[nodiscard]] ryn::runtime::ComponentId root() const noexcept;
    [[nodiscard]] GalleryPageTicket ticket() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::size_t cached_pages() const noexcept;
    [[nodiscard]] const GalleryPageDraft& draft() const;
    void set_anchor(GalleryPageAnchor anchor);
    void shutdown() noexcept;

private:
    struct Request final {
        GalleryPageRoute route;
        std::optional<std::size_t> history_index;
    };

    void mount(const ryn::Content& content);
    void retire(bool save);
    [[nodiscard]] bool owns(ryn::runtime::ComponentId component) const;
    void update_history(const Request& request);

    ReferenceSurfaceHost* surfaces_;
    GalleryPageModel* model_;
    ryn::runtime::FrameRequestState* frames_;
    ryn::runtime::ComponentId parent_;
    Factory factory_;
    std::optional<Request> requested_;
    std::optional<GalleryPageContent> active_;
    GalleryPageRoute active_route_;
    ryn::runtime::ComponentId root_;
    std::map<std::string, GalleryPageDraft> drafts_;
    std::vector<GalleryPageRoute> history_;
    std::size_t history_index_{};
    std::shared_ptr<GalleryPageTicketState> tickets_{std::make_shared<GalleryPageTicketState>()};
    std::string last_error_;
    bool processing_{};
    bool failed_{};
};

[[nodiscard]] std::string gallery_page_state_key(const GalleryPageRoute& route);

} // namespace rynui::example
