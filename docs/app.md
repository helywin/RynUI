# App 与 Notice 服务

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

此阶段覆盖 App/Notice 基础。`rynui.app_service_*`、`rynui.app_public_api`、`rynui.i18n_service` 的实际 Windows 结果见[阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)。Modal/Drawer/Popconfirm 动态服务与 async confirm 属于后续增量；107 App 和 108 服务完整任务继续跟踪，没有空接口占位。
