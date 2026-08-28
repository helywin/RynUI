## Purpose

定义 RynUI 在 Linux 原生 Wayland 下解析和验证 patched libdecor、传递交互缩放状态并持续确认 configure/提交可见帧的行为合同，使窗口缩放不依赖失焦且不破坏显式依赖模式和跨输出坐标语义。

## ADDED Requirements

### Requirement: 可复现的 patched libdecor 输入
Linux `BUNDLED` 构建 SHALL 使用锁定的 libdecor release archive、SHA256、license 和仓库内可审计 patch；补丁 MUST 只扩展明确声明的兼容能力，不得通过伪造第三方版本号开放其他未实现 API。

#### Scenario: 配置 Linux BUNDLED 构建
- **WHEN** 开发者通过 Linux CMake preset 选择 `BUNDLED` 依赖模式
- **THEN** 构建 MUST 从锁定输入生成 patched libdecor core 与可用 decoration plugin，并让 SDL3 只使用该 build-local 组合

#### Scenario: 锁定内容或补丁不匹配
- **WHEN** libdecor archive 校验失败、patch 上下文不再匹配或必需平台开发包缺失
- **THEN** configure 或 build MUST fail-fast，并在诊断中指出失败的依赖、补丁或平台服务

#### Scenario: 配置 SYSTEM 构建
- **WHEN** 开发者显式选择 `SYSTEM` 依赖模式
- **THEN** 构建 MUST 只使用集成方提供的 SDL3/libdecor 组合，不得静默下载 bundled libdecor，也不得把未验证组合标记为已应用本修复

#### Scenario: 配置非 Linux 构建
- **WHEN** 开发者在 Windows/MSVC 等非 Linux 平台配置 RynUI
- **THEN** 构建 MUST 不下载、不构建也不链接 libdecor，现有 SDL3 平台行为保持不变

### Requirement: libdecor 交互缩放状态传递
patched libdecor SHALL 把 Wayland compositor 提供的 `XDG_TOPLEVEL_STATE_RESIZING` 作为 resize 状态交给 SDL3；bundled SDL3 MUST 读取该状态，而不得因 release version 小于 0.3.0 忽略已经存在的能力。

#### Scenario: compositor 开始交互缩放
- **WHEN** 原生 Wayland compositor 的 configure states 包含 `XDG_TOPLEVEL_STATE_RESIZING`
- **THEN** SDL3 MUST 在处理对应 configure 时识别交互缩放状态并采用 resize 路径

#### Scenario: configure 不包含缩放状态
- **WHEN** configure states 不包含 `XDG_TOPLEVEL_STATE_RESIZING`
- **THEN** SDL3 MUST 不把窗口误判为正在交互缩放，maximize、fullscreen、suspended 与其他 0.3 API gate 保持原有语义

### Requirement: configure 确认与帧提交持续前进
原生 Wayland interactive resize 期间，RynUI 的 bundled SDL3/libdecor 路径 SHALL 合并过时 configure，并在受 compositor frame pacing 约束的下一次可提交机会确认最新 configure、更新窗口几何并请求可见帧；处理 MUST 不依赖窗口失焦、重新获得焦点或拖动结束。

#### Scenario: 持续快速拖动窗口边缘
- **WHEN** compositor 在一次 interactive resize 中连续发送多个不同尺寸的 configure
- **THEN** 窗口 MUST 在拖动期间持续产生尺寸和可见帧更新，且最新 configure MUST 前进而不等待 focus event

#### Scenario: 一帧前收到多个 configure
- **WHEN** 下一次 frame callback 前收到多个 interactive-resize configure
- **THEN** 实现 MUST 只保留和确认最新有效 configure，不得重复确认同一 serial，也不得为每个过时尺寸建立无界待处理队列

#### Scenario: 缩放结束
- **WHEN** compositor 发送不再包含 resize 状态的 configure
- **THEN** 最终窗口几何 MUST 被确认并稳定呈现，后续闲置状态 MUST 恢复 RynUI 的按需帧行为

#### Scenario: 窗口在待确认状态销毁
- **WHEN** 窗口仍持有待处理 configure 时关闭或初始化失败
- **THEN** 该 configure 的所有权 MUST 被释放且不得发生重复确认、悬空访问或平台资源泄漏

### Requirement: 原生 Wayland 跨输出验收
修复 SHALL 在受影响 Linux/GNOME 原生 Wayland 环境通过真实窗口验收，自动测试、XWayland 或单输出结果 MUST 不替代该证据。

#### Scenario: 高刷新率分数缩放输出
- **WHEN** 验收人员在 240 Hz、display scale 1.333 的原生 Wayland 输出快速连续调整 `rynui_minimal` 和公开示例窗口尺寸
- **THEN** 窗口 MUST 在拖动期间持续更新、不得等待失焦，pointer 命中与 logical viewport MUST 保持对应

#### Scenario: 低刷新率整数缩放输出
- **WHEN** 验收人员在 60 Hz、display scale 1.0 的原生 Wayland 输出执行同样操作
- **THEN** 窗口 MUST 跟随连续 configure 更新且不得出现拖动结束后继续追赶旧尺寸的明显动画队列

#### Scenario: 跨输出拖动后缩放
- **WHEN** 窗口在两个不同刷新率与缩放输出之间移动后立即调整尺寸
- **THEN** drawable、logical viewport、pointer 坐标和可见内容 MUST 使用当前输出指标，且 resize 前进保证保持成立
