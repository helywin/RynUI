# App 与窗口服务

`ryn/app.hpp` 提供 retained `App`、typed `AppProps` / `AppContent`、`AppRef` 和 `useApp()`。App 自动挂载真实 Message/Notification holders；可以在事件中直接打开、更新、关闭富内容通知，不需要另写 Holder。

```cpp
ryn::App(ryn::AppProps{}, ryn::AppContent{[] {
    const auto app = ryn::useApp();
    ryn::Button(ryn::ButtonProps{}.onClick([app] {
        const auto notice = app.notification().openAsync(
            u8"save", ryn::NotificationProps{}.title(u8"Saved").duration(0));
        // Keep notice in application state to update(), close(), or observe closed().
    }), [] { ryn::Text(u8"Save"); });
}});
```

完整公开头示例在 [examples/app_notice](../examples/app_notice/app_notice.cpp)，包含语言切换、保留 Input、状态更新与关闭结果；将 `content(model)` 交给通常的窗口 host。

`AppProps::ref(ref)` 捕获 App 所在的配置。`useApp()` 在 mount 内查最近 App，同时捕获调用处活的 Theme/config；把结果保存在事件闭包中，不在事件里再次调用 hook。嵌套 Provider 下捕获的 ref 会继续响应语言、方向和 Theme 更新。普通 String 保持原样，`useI18n().text(...)` 的 Binding 继续响应。

每个 ref 都指向明确窗口，所有 UI 请求只接受其 owner thread。另一个窗口的事件可以调用这个明确目标；不得向目标 host 注入另一个窗口的 captured context。没有进程级“当前 App”。同一 ref 不能同时绑定两个活 App；已绑定后复制的 ref 固定原代际，原始显式 ref 重新挂载不会复活旧副本。未绑定/过期请求返回 false，`openAsync` 返回已经 Cancelled 的 future。

同 key `open` 和 `NoticeHandle::update` 保留没有提供替换 slot 的富内容身份。不同捕获上下文不能用同 key 改写现有 entry，拒绝发生在 props/slots 连接之前；可使用另一 key 或先关闭旧 entry。handle 的更新和关闭只作用于原 entry 代际。替换 slot 抛错保留旧 region 和旧 completion，其他 prop 失败仍沿用原 Notice 的失败退役语义，不承诺任意用户回调的全属性回滚。

Notice 实例挂在真实捕获来源子树下，holder 管理 key/容量/future；Modal 内来源因此沿用共同的输入域与绘制层次，不能绕过顶层 fullscreen Spin。来源、holder、App 或窗口退役都会释放自己的内容与输入资源，并将其未完成 future 置为 Cancelled。正常关闭在实际退出边界置为 Closed；调用错误置为 Failed。future 可以在另一线程等待，UI 更新/关闭仍只在 owner thread；`closed()` 不轮询或创建后台线程。

关闭先提交退役与 future，再通知 `onClose`；重入同 key 创建新 entry 不会被旧清理误删。clear 会清理整批旧 entry，再向调用者重抛第一个 close callback 异常。硬退役不调用依赖已销毁 UI 的 close callback。隐藏来源暂停 Notice timer；失活窗口继续计算真实到期时间并停止持续动画帧。静态、关闭或退役后没有 Notice deadline。

App/Notice 基础的实际 Windows 结果见[阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)。以下 panel 服务、异步动作及受影响旧合同已通过正式 MSVC HEADLESS Debug/Release 各 74 个不同入口；实际范围和平台边界见[服务阶段结果](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/panel-services-windows.md)。

## Modal、Drawer 与 Popconfirm

`AppRef::modal()` 提供 `confirm/info/success/error/warning`，返回 `ModalHandle`。info 和其他状态使用实际 Icon 和单确认按钮；confirm 保留双按钮。它们挂载同一个真实 Modal，复用 WindowOverlayStack。`drawer().open` 返回 `DrawerHandle`，挂载真实 Drawer，默认没有确认页脚。每次调用创建独立实例，handle 只指向该 retained 代际。

```cpp
const auto handle = app.modal().confirm(
    ryn::ModalProps{}.title(tr.text(title)).onOkAsync(
        ryn::ConfirmAction::synchronous([] {
            return save_draft() ? ryn::ConfirmActionResult{}
                : ryn::ConfirmActionResult{ryn::ConfirmActionDecision::KeepOpen};
        })),
    ryn::ModalContent{[] { ryn::Input(ryn::InputProps{}); }});
handle.update({.width = ryn::Prop<float>{600.0F}});
// 保存 handle；closed() 可在另一线程等待。
```

`ModalUpdate` 支持 title/ok_text/cancel_text/width/content，`DrawerUpdate` 支持 title/width/height/loading/content，`PopconfirmUpdate` 支持 title/description/ok_text/cancel_text。未提供 content 保留正文；显式 content 先完整挂载候选，再替换旧区。候选失败保留旧区和原 future；尺寸非法在发布前拒绝。更新不是任意用户副作用下的全属性事务。typed slots 在创建时声明，后续文字 patch 不覆盖富 slot。

Popconfirm 需要真实保留锚点：先挂载 `Popconfirm(PopconfirmProps{}.serviceAnchor(anchor), trigger)`，trigger 的 Button 事件调用 `app.popconfirm().confirm(anchor, props, slots)`。`serviceAnchor` 保留原生 Button 激活，禁止自动再开 declarative popup；不能同时声明 open/defaultOpen。普通 `.ref(anchor)` 仍可获取 declarative ref，但受控、已打开或已被另一服务使用的锚点拒绝服务接管。锚点必须属于同一窗口与最近 App，调用处已有 overlay 时须属于该 overlay；不穿过嵌套 App 或跳到另一 overlay family。服务使用锚点当前真实几何，并在锚点子树中挂载真实 popup；不复制 trigger，不保存鼠标坐标。关闭后锚点继续可用，旧 ref 副本不会因原 ref 重绑而复活。失效服务的新建请求抛出明确错误，已有 handle 的 update/close/destroy 返回 false。

所有 panel 都保留调用处 Theme/config 活句柄。服务挂在调用来源子树（Popconfirm 挂在锚点子树，同时关联调用来源），服务中 `useApp()` 仍找到真实 App；Modal 内调用继续属于该 overlay family。更新、动作和完成分别检查窗口、App、来源、entry 和 action 代际；语言修订不使业务票据失效。默认 footer 随 Provider 更新，String 是 literal，Binding 继续响应。

`closed()` 返回 `shared_future<PanelCloseResult>`，区分 Confirmed、Cancelled、Closed（Drawer 显式 close）、Destroyed、OwnerDisposed、Failed，并保留适用的 Modal/Drawer cancel reason。动作 KeepOpen、业务错误或投递错误保持打开且 future pending。`close` 是应用显式取消/关闭，`destroy` 是硬销毁；两者终止 pending 动作。普通用户入口在 pending 时阻止重复确认与取消。服务拥有 open；传入 props 的 open 被服务替代。原 declarative open 仍由应用控制。

Drawer 普通关闭先撤销整个 overlay 输入，保留退出画面；handle 的 bound/update/重复 close 随即失效，但 entry 继续属于 owner 清理集合。future 在有效退出达到终点、终态 CPU scene 成功同步后的安全通知阶段完成。Theme motion 关闭、reduced、隐藏或失活走有限静态收口，同样不承诺 close() 返回前 future ready；这不等待 GPU fence 或显示器呈现。destroy、来源/App/窗口退役和失败立即取消退出并一次结算，旧完成桥不能覆盖硬退役结果。关闭期间保留既有响应式标题、尺寸与 loading 连接。

启用 `ModalProps::animated` 的 Modal 采用同一终点结算规则；未启用的 Modal 和 Popconfirm 保留即时退役边界。Modal 的 responsive width 来源继续保持响应，显式 `ModalUpdate::width` 则替换该来源。最终 finish 顺序保持移出服务集合、撤输入/销毁旧 retained 资源、完成 future，再通知取消、afterOpenChange(false)、afterClose。两个 after 回调即使前者抛错也按顺序尝试，内部清理不依赖回调；owner 已退役则不再通知。整批 owner 退役先使所有 action 无效，再执行任一用户 cancel。异常取消不会中断余下清理。

## ConfirmAction

`ryn/confirm_action.hpp` 的 `ConfirmAction(provider, OwnerThreadPost, timeout)` 接受 provider 返回 Cancel，完成结果为 Close/KeepOpen 与可选 typed error。默认 30 秒，允许 (0, 24h]；timeout 使用窗口单调时钟。同步动作可用 `ConfirmAction::synchronous`，抛错转 ProviderThrew。Modal 的 `onOkAsync`、Popconfirm 的 `onConfirmAsync` 共用小会话；`onActionError` 收到 Business/ProviderThrew/DispatchFailed/Timeout，不自动创建错误弹窗。业务错误正文不被翻译。

provider 可同步/重复/worker 完成，只有第一个 completion 被接受。worker 只访问 atomic ticket 和 dispatcher；不访问 Signal、host 或 FrameRequestState。dispatcher 必须可靠安排 owner-thread 调用。post 抛错或在错误线程执行会标记 DispatchFailed；下一次正常 owner tick 消费，最迟已登记的有限 deadline 恢复 loading、调用 cancel 并保持可重试。dispatcher 丢弃闭包由 Timeout 恢复；窗口 event loop 必须继续处理 deadline。正常完成或取消立即移除 deadline，不轮询 future，也不新增线程。

源码入口为 `rynui.confirm_action`、`rynui.app_panel_*`、`rynui.drawer_motion_*`、`rynui.modal_completion_*`，并扩展原 `rynui.i18n_service`；公开头 TU 和 Gallery App 本页提供真实调用。Popconfirm motion、其余组件缺口和原生平台结果继续由各组件任务跟踪；新增测试源码不等于平台验收通过。
