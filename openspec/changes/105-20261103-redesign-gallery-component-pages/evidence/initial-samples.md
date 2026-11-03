# 首批真实样例提取

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 从旧混合构造函数提取 Button、Input/Search/Password/TextArea/OTP 和 TreeSelect，登记 14 个样例组与实际构造函数、7 个有效公开头文件路径。只执行活动 page/family 的工厂，其余工厂调用数为零；Upload 不构造仿制控件。
- 构建 `rynui_gallery_sample_page_tests`、`rynui_gallery_page_controller_tests`、`rynui_token_gallery_frame_tests`：通过。定向 CTest `^rynui\.gallery_(page_controller|sample_page)$`：2/2 通过；旧 frame 二进制 `--input-only` 提取兼容回归：1/1 通过。
- 新页面夹具通过 mount 时登记的 page/sample/test ID 定位目标。验证 controlled/uncontrolled Input、TextArea 和 Search 草稿；Password/OTP 离组清空；TreeSelect 的选择/备注/Form/展开数据与 popup transient 分离；保留 Button 类型/变体/ghost/block/延迟 loading/ref、Search Enter/Tab/Space、OTP 分发、TreeSelect 接受/拒绝选择及关闭和 Form required/submit 行为。
- 验证 preedit 不提交业务值、Theme/resize 保留编辑器与 scene 身份、模拟 host input area 更新。Password/OTP 的滚动锚点与敏感值分开保存。

仅完成任务 3.1 的提取及其独立验证。既有全 Gallery frame 用例的完整迁移（3.2/3.6）、其他组件、文档 shell 和真实原生平台入口仍未完成。这里的 C++ 函数/头文件映射不表示代码展开 UI 已实现。没有运行桌面窗口、真实 IME 或 GPU；原始输出只在 `out/105-implementation/20261103/`。
