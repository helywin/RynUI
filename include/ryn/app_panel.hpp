#pragma once

#include <ryn/modal.hpp>
#include <ryn/drawer.hpp>
#include <ryn/popconfirm.hpp>
#include <future>

namespace ryn {
namespace detail {
struct PanelServiceContext;
struct PanelHandleState;
class AppPanelService;
} // namespace detail

enum class PanelCloseKind { Confirmed, Cancelled, Closed, Destroyed, OwnerDisposed, Failed };

struct PanelCloseResult final {
    PanelCloseKind kind{PanelCloseKind::OwnerDisposed};
    std::optional<ModalCancelReason> modal_cancel;
    std::optional<DrawerCancelReason> drawer_cancel;
    friend bool operator==(const PanelCloseResult&, const PanelCloseResult&) = default;
};

// Patches preserve the body unless content is explicitly supplied. A new Prop
// remains reactive; an explicit String stays literal. Owner/open are service owned.
struct ModalUpdate final {
    std::optional<Prop<String>> title;
    std::optional<Prop<String>> ok_text;
    std::optional<Prop<String>> cancel_text;
    std::optional<Prop<float>> width;
    std::optional<ModalContent> content;
};

struct DrawerUpdate final {
    std::optional<Prop<String>> title;
    std::optional<Prop<float>> width;
    std::optional<Prop<float>> height;
    std::optional<Prop<bool>> loading;
    std::optional<DrawerContent> content;
};

struct PopconfirmUpdate final {
    std::optional<Prop<String>> title;
    std::optional<Prop<String>> description;
    std::optional<Prop<String>> ok_text;
    std::optional<Prop<String>> cancel_text;
};

class ModalHandle final {
public:
    ModalHandle() = default;
    [[nodiscard]] bool bound() const;
    bool update(ModalUpdate) const;
    bool close(ModalCancelReason = ModalCancelReason::CancelButton) const;
    bool destroy() const;
    [[nodiscard]] std::shared_future<PanelCloseResult> closed() const;

private:
    friend class detail::AppPanelService;
    std::shared_ptr<detail::PanelHandleState> state_;
};

class DrawerHandle final {
public:
    DrawerHandle() = default;
    [[nodiscard]] bool bound() const;
    bool update(DrawerUpdate) const;
    bool close(DrawerCancelReason = DrawerCancelReason::CloseButton) const;
    bool destroy() const;
    [[nodiscard]] std::shared_future<PanelCloseResult> closed() const;

private:
    friend class detail::AppPanelService;
    std::shared_ptr<detail::PanelHandleState> state_;
};

class PopconfirmHandle final {
public:
    PopconfirmHandle() = default;
    [[nodiscard]] bool bound() const;
    bool update(PopconfirmUpdate) const;
    bool close() const;
    bool destroy() const;
    [[nodiscard]] std::shared_future<PanelCloseResult> closed() const;

private:
    friend class detail::AppPanelService;
    std::shared_ptr<detail::PanelHandleState> state_;
};

class ModalServiceRef final {
public:
    [[nodiscard]] bool bound() const;
    [[nodiscard]] ModalHandle confirm(ModalProps, ModalContent, ModalSlots = {}) const;
    [[nodiscard]] ModalHandle info(ModalProps, ModalContent, ModalSlots = {}) const;
    [[nodiscard]] ModalHandle success(ModalProps, ModalContent, ModalSlots = {}) const;
    [[nodiscard]] ModalHandle error(ModalProps, ModalContent, ModalSlots = {}) const;
    [[nodiscard]] ModalHandle warning(ModalProps, ModalContent, ModalSlots = {}) const;

private:
    friend class AppRef;
    std::shared_ptr<detail::PanelServiceContext> context_;
};

class DrawerServiceRef final {
public:
    [[nodiscard]] bool bound() const;
    [[nodiscard]] DrawerHandle open(DrawerProps, DrawerContent, DrawerSlots = {}) const;

private:
    friend class AppRef;
    std::shared_ptr<detail::PanelServiceContext> context_;
};

class PopconfirmServiceRef final {
public:
    [[nodiscard]] bool bound() const;
    [[nodiscard]] PopconfirmHandle confirm(const PopconfirmRef&, PopconfirmProps, PopconfirmSlots = {}) const;

private:
    friend class AppRef;
    std::shared_ptr<detail::PanelServiceContext> context_;
};
} // namespace ryn
