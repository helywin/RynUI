## Purpose

定义 RynUI 最小应用在受支持桌面平台上的构建、窗口生命周期、事件处理和 GPU 帧提交行为，为后续 UI 能力提供可重复验证的运行基线。

## ADDED Requirements

### Requirement: 可配置的 C++20 工程
RynUI 工程 SHALL 能够通过仓库提交的 `CMakePresets.json` 和 `Ninja Multi-Config` 完成配置、构建和测试，公开 C++ API MUST 位于 `ryn` 命名空间。

#### Scenario: 配置并构建开发目标
- **WHEN** 开发者在受支持的工具链环境中执行文档化的 configure 和 build 命令
- **THEN** 核心库、测试目标和最小示例 MUST 成功生成

#### Scenario: 使用公开命名空间
- **WHEN** 下游示例包含 RynUI 公开头文件并调用公开 API
- **THEN** 该 API MUST 通过 `ryn` 命名空间访问，且不得要求使用内部命名空间

#### Scenario: Windows 使用 MSVC preset
- **WHEN** 开发者在 Windows Visual Studio Developer Environment 中执行 `windows-msvc` configure preset
- **THEN** 工程 MUST 使用 MSVC x64 和 `Ninja Multi-Config` 完成配置，并且非 MSVC 工具链 MUST 在 configure 阶段失败

#### Scenario: Linux 选择受支持工具链
- **WHEN** 开发者在 Linux 中选择 `linux-gcc` 或 `linux-clang` configure preset
- **THEN** 工程 MUST 使用对应编译器和 `Ninja Multi-Config` 完成配置

### Requirement: 窗口与 GPU 生命周期
最小应用 SHALL 创建可见窗口、完成 GPU 初始化、处理关闭事件，并在正常退出或初始化失败时释放已取得的资源。

#### Scenario: 正常启动与退出
- **WHEN** 用户启动最小应用且窗口与 GPU 初始化成功
- **THEN** 应用 MUST 显示窗口、提交至少一个有效帧，并在用户关闭窗口后正常退出

#### Scenario: 初始化中途失败
- **WHEN** 窗口创建后发生 GPU 初始化失败
- **THEN** 应用 MUST 返回失败结果并释放此前创建的窗口和平台资源

### Requirement: 受支持平台验收
本 change 的运行基线 SHALL 在 Windows 与 Linux 上分别通过构建和真实窗口验收。

#### Scenario: Windows 真实窗口验收
- **WHEN** 验收人员在 Windows 环境运行最小应用
- **THEN** 窗口创建、帧提交、事件响应和退出流程 MUST 全部通过

#### Scenario: Linux 真实窗口验收
- **WHEN** 验收人员在 Linux 环境运行最小应用
- **THEN** 窗口创建、帧提交、事件响应和退出流程 MUST 全部通过

### Requirement: 按需帧提交
应用在没有输入、动画、状态更新或显式重绘请求时 MUST 停止持续满帧提交，并在出现新工作时恢复处理。

#### Scenario: 窗口进入闲置状态
- **WHEN** 窗口没有待处理的输入、动画、状态更新或重绘请求
- **THEN** 帧提交计数 MUST 不再按显示刷新率持续增长

#### Scenario: 闲置后收到输入
- **WHEN** 闲置窗口收到需要改变可见结果的输入事件
- **THEN** 应用 MUST 调度并提交包含该变化的新帧
