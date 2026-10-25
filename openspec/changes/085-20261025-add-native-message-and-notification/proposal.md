# Proposal

## Why

窗口中的按钮事件需要能打开、更新和关闭真实的消息与通知。目前已有 Alert、Button、Input 和 retained slot，但缺少窗口作用域的动态 notice 入口与到期调度。

## What Changes

- 新增 MessageHolder / NotificationHolder、typed Props/slots 与安全 Ref；以显式 key 打开、局部更新、关闭和清空。
- Message 顶部居中；Notification 四角堆叠，提供标题、描述、真实关闭 Button 和 action slot。支持 success/info/warning/error/loading，loading 首版为静态图标。
- 使用既有窗口辅助 tick/deadline 处理正 duration；0 持久。默认分别为 3 秒和 4.5 秒；hover、失活均不暂停单调计时。
- 完整接入 Theme 链，保留普通 reactive 更新下的 slot、Input 草稿和 refs，验证回调卸载、同 key 重入与边界。
- 新增独立 API 文档与针对性 headless 测试。不增加 singleton、事件总线、promise、动画、全局服务、Gallery 或新依赖。

## Capabilities

### New Capabilities

- `native-message-notification`: 窗口作用域的动态消息通知、保留内容、到期与输入生命周期合同。

### Modified Capabilities

无。

## Impact

新增公开头、内部 notice host/model、窗口服务注册、Theme typed token/override/派生/校验/继承/identity/序列化及五份 golden，新增 tests 和 docs/message-notification.md。Core、logical scene v3、GPU ABI 和 renderer 路径保持现有边界。主要风险为同步用户回调导致代际失效、动态 slot 的清理、有限视口裁剪与大型 Theme 栈占用。验收使用 Windows MSVC Ninja Multi-Config headless Debug/Release 的去重受影响集合，不代表 GPU、真实窗口、Linux、系统 IME 或 AT 已通过。
