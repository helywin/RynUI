#pragma once

#include "component/app_component.hpp"
#include "component/confirm_action.hpp"
#include <ryn/app_panel.hpp>
#include <ryn/icon.hpp>

namespace ryn::detail {
struct PanelEntry;

struct PanelServiceContext final {
    std::weak_ptr<AppBinding> binding;
    runtime::CapturedComponentContext captured;
};

struct PanelHandleState final {
    std::weak_ptr<PanelEntry> entry;
    std::shared_future<PanelCloseResult> future;
};

class AppPanelService final {
public:
    static bool live(const std::shared_ptr<PanelServiceContext>&);
    static bool live(const std::shared_ptr<PanelHandleState>&);
    static ModalHandle modal(const std::shared_ptr<PanelServiceContext>&, ModalProps, ModalContent, ModalSlots,
                             std::optional<IconName> = {});
    static DrawerHandle drawer(const std::shared_ptr<PanelServiceContext>&, DrawerProps, DrawerContent, DrawerSlots);
    static PopconfirmHandle popconfirm(const std::shared_ptr<PanelServiceContext>&, const PopconfirmRef&,
                                       PopconfirmProps, PopconfirmSlots);
    static bool update(const std::shared_ptr<PanelHandleState>&, ModalUpdate);
    static bool update(const std::shared_ptr<PanelHandleState>&, DrawerUpdate);
    static bool update(const std::shared_ptr<PanelHandleState>&, PopconfirmUpdate);
    static bool close(const std::shared_ptr<PanelHandleState>&, PanelCloseResult);
    static std::shared_future<PanelCloseResult> closed(const std::shared_ptr<PanelHandleState>&);
    // Called synchronously by retained owner cleanup, before descendant scope cleanup.
    static void retire_owner(const std::shared_ptr<AppBinding>&,
                             std::optional<runtime::ComponentId> source = {}) noexcept;
};
} // namespace ryn::detail
