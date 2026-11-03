#include "gallery_page_controller.hpp"

#include <ryn/button.hpp>
#include <ryn/flex.hpp>
#include <ryn/text.hpp>

#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

namespace rynui::example {
namespace {

GalleryPageRoute page_identity(GalleryPageRoute route) {
    route.sample_identity.clear();
    route.section.reset();
    return route;
}

bool sensitive(const GalleryPageRoute& route) {
    return route.kind == GalleryPageKind::component && route.component_identity == "ant.component.input" &&
           (route.family == GalleryInputFamily::password || route.family == GalleryInputFamily::otp);
}

const GalleryPageDraft empty_draft;

} // namespace

std::string gallery_page_state_key(const GalleryPageRoute& route) {
    return std::to_string(static_cast<int>(route.kind)) + "/" + route.component_identity + "/" +
           std::to_string(static_cast<int>(route.family));
}

bool GalleryPageTicket::valid() const noexcept {
    const auto state = state_.lock();
    return state && state->alive && state->accepting && state->generation == generation_;
}

GalleryPageController::GalleryPageController(ReferenceSurfaceHost& surfaces, GalleryPageModel& model,
                                             ryn::runtime::FrameRequestState& frames, ryn::runtime::ComponentId parent,
                                             Factory factory)
    : surfaces_(&surfaces), model_(&model), frames_(&frames), parent_(parent), factory_(std::move(factory)),
      history_{page_identity(model.current())} {
    if (!factory_ || !surfaces.application().components().contains(parent)) {
        throw std::invalid_argument("Gallery page controller requires a mounted parent and a factory");
    }
}

GalleryPageController::~GalleryPageController() {
    shutdown();
}

bool GalleryPageController::request(GalleryPageRoute route) {
    if (!tickets_->alive || !model_->valid(route) || (requested_ && requested_->route == route)) {
        return false;
    }
    requested_ = Request{std::move(route), std::nullopt};
    frames_->request_frame();
    return true;
}

bool GalleryPageController::owns(ryn::runtime::ComponentId component) const {
    auto& components = surfaces_->application().components();
    while (components.contains(component)) {
        if (component == root_) {
            return true;
        }
        const auto parent = components.parent(component);
        if (!parent) {
            break;
        }
        component = *parent;
    }
    return false;
}

void GalleryPageController::retire(bool save) {
    tickets_->accepting = false;
    ++tickets_->generation;
    std::exception_ptr failure;
    if (active_ && save && !sensitive(active_route_)) {
        try {
            auto snapshot = drafts_[gallery_page_state_key(active_route_)];
            if (active_->save) {
                active_->save(snapshot);
            }
            drafts_[gallery_page_state_key(active_route_)] = std::move(snapshot);
        } catch (...) {
            failure = std::current_exception();
        }
    } else if (sensitive(active_route_)) {
        const auto found = drafts_.find(gallery_page_state_key(active_route_));
        if (found != drafts_.end()) {
            found->second.values.clear();
        }
    }
    try {
        auto& application = surfaces_->application();
        auto& services = application.services();
        const auto declarations = services.interactions().declaration_order();
        const std::vector<ryn::input::InteractionId> interactions(declarations.begin(), declarations.end());
        for (const auto id : interactions) {
            const auto* record = services.interactions().find(id);
            if (record && owns(record->component)) {
                services.pointer().cancel_interaction(id);
            }
        }
        const auto focus = services.focus().state().focused;
        if (focus) {
            const auto* record = services.interactions().find(*focus);
            if (record && owns(record->component)) {
                static_cast<void>(services.focus().clear_focus());
            }
        }
        if (active_ && active_->before_retire) {
            active_->before_retire();
        }
    } catch (...) {
        if (!failure) {
            failure = std::current_exception();
        }
    }
    if (surfaces_->application().components().contains(root_)) {
        static_cast<void>(surfaces_->destroy(root_));
    }
    root_ = {};
    active_.reset();
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void GalleryPageController::mount(const ryn::Content& content) {
    auto& components = surfaces_->application().components();
    const auto previous = components.children(parent_).size();
    surfaces_->append_slot(parent_, ryn::Content{[content] {
                               ryn::Flex(ryn::FlexProps{}.vertical(true).align(ryn::FlexAlign::Start),
                                         [content] { ryn::detail::SlotContentAccess::function(content)(); });
                           }});
    const auto& children = components.children(parent_);
    if (children.size() != previous + 1) {
        throw std::logic_error("Gallery page must have exactly one owned root");
    }
    root_ = children.back();
}

void GalleryPageController::update_history(const Request& request) {
    const auto route = page_identity(request.route);
    if (request.history_index) {
        history_index_ = *request.history_index;
    } else if (history_[history_index_] != route) {
        history_.resize(history_index_ + 1);
        history_.push_back(route);
        history_index_ = history_.size() - 1;
    }
}

bool GalleryPageController::process() {
    if (!requested_ || processing_ || !tickets_->alive) {
        return false;
    }
    auto request = std::move(*requested_);
    requested_.reset();
    const bool same_page = page_identity(request.route) == page_identity(active_route_);
    if (surfaces_->application().components().contains(root_) && same_page && !failed_) {
        return model_->select(std::move(request.route));
    }
    processing_ = true;

    struct Reset final {
        bool& value;

        ~Reset() {
            value = false;
        }
    } reset{processing_};

    static_cast<void>(model_->select(request.route));
    update_history(request);
    failed_ = false;
    last_error_.clear();
    try {
        retire(true);
        active_route_ = request.route;
        tickets_->accepting = true;
        auto page = factory_(request.route, draft());
        mount(page.content);
        active_ = std::move(page);
    } catch (...) {
        try {
            throw;
        } catch (const std::exception& error) {
            last_error_ = error.what();
        } catch (...) {
            last_error_ = "Gallery page construction failed";
        }
        failed_ = true;
        tickets_->accepting = false;
        ++tickets_->generation;
        active_route_ = request.route;
        if (surfaces_->application().components().contains(root_)) {
            static_cast<void>(surfaces_->destroy(root_));
        }
        root_ = {};
        active_.reset();
        tickets_->accepting = true;
        const auto lease = ticket();
        mount(ryn::Content{[this, lease] {
            ryn::Text(u8"此页暂时无法显示");
            ryn::Button(ryn::ButtonProps{}.onClick([this, lease] {
                if (lease.valid()) {
                    this->request(model_->current());
                }
            }),
                        [] { ryn::Text(u8"重试"); });
            ryn::Button(ryn::ButtonProps{}.onClick([this, lease] {
                if (lease.valid()) {
                    this->request({});
                }
            }),
                        [] { ryn::Text(u8"返回组件总览"); });
        }});
    }
    frames_->request_frame();
    return true;
}

bool GalleryPageController::back() {
    if (!can_back()) {
        return false;
    }
    requested_ = Request{history_[history_index_ - 1], history_index_ - 1};
    frames_->request_frame();
    return true;
}

bool GalleryPageController::forward() {
    if (!can_forward()) {
        return false;
    }
    requested_ = Request{history_[history_index_ + 1], history_index_ + 1};
    frames_->request_frame();
    return true;
}

bool GalleryPageController::can_back() const noexcept {
    return tickets_->alive && history_index_ > 0;
}

bool GalleryPageController::can_forward() const noexcept {
    return tickets_->alive && history_index_ + 1 < history_.size();
}

bool GalleryPageController::pending() const noexcept {
    return requested_.has_value();
}

bool GalleryPageController::failed() const noexcept {
    return failed_;
}

const std::string& GalleryPageController::last_error() const noexcept {
    return last_error_;
}

ryn::runtime::ComponentId GalleryPageController::root() const noexcept {
    return root_;
}

GalleryPageTicket GalleryPageController::ticket() const noexcept {
    GalleryPageTicket result;
    result.state_ = tickets_;
    result.generation_ = tickets_->generation;
    return result;
}

std::uint64_t GalleryPageController::generation() const noexcept {
    return tickets_->generation;
}

std::size_t GalleryPageController::cached_pages() const noexcept {
    return drafts_.size();
}

const GalleryPageDraft& GalleryPageController::draft() const {
    const auto found = drafts_.find(gallery_page_state_key(model_->current()));
    return found == drafts_.end() ? empty_draft : found->second;
}

void GalleryPageController::set_anchor(GalleryPageAnchor anchor) {
    if (!std::isfinite(anchor.distance)) {
        throw std::invalid_argument("Gallery page anchor distance must be finite");
    }
    drafts_[gallery_page_state_key(model_->current())].anchor = std::move(anchor);
}

void GalleryPageController::shutdown() noexcept {
    if (!tickets_->alive) {
        return;
    }
    tickets_->alive = false;
    requested_.reset();
    try {
        retire(false);
    } catch (...) {
        // Teardown still retires the subtree before propagating any callback failure.
    }
}

} // namespace rynui::example
