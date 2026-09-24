# Design

## Context

见 [proposal.md](proposal.md)。010 已提供单行 `InputComponentHost`、编辑器、IME 会话和剪贴板；013 已把窗口服务从 Button 宿主抽出；011 的 Switch/Checkbox 已使用该服务。Ant Design 6.6.5 的 `Search.tsx` 由 Input、Button 与紧凑布局组成，本地来源 commit 和关键文件 SHA256 将在本 change 来源合同中固定。RynUI 当前没有 React/CSS 语义层，首批只实现已在 proposal 标明的可验证子集。

## Goals / Non-Goals

**Goals:**

- 让 Search 的编辑、按压、焦点、loading、Theme、scene 与帧同步完全由现有 Input/Button/布局机制提供。
- 让按钮点击与 Enter 在 controlled/uncontrolled 两种模式下取得同一 committed value；支持动态 Prop，不因文本和颜色更新重新执行无关 slot。
- 以同窗 headless 和真实窗口验证组合生命周期，而不是通过另建 Search 编辑器通过单体测试。

**Non-Goals:**

- 不扩展多行编辑、Password/OTP/InputNumber、allowClear、任意图标或 ReactNode 等价物。
- 不新增公开 Control 基类、视觉 Modifier 或第二份 IME/clipboard 端口。
- 不把 Search 首批支持状态描述为 Ant Design `Input.Search` 的完整 Props/视觉等价。

## Decisions

### 1. 使用组合函数和窄状态桥

公开 `SearchProps` 持有 typed Input 参数、`enterButton`、`loading`、onSearch 与外部布局；可选 `SearchButtonContent` typed slot 提供按钮文字。`Search()` 在挂载前验证 value/defaultValue 冲突和 size 等非法值，然后创建单个内部 `SearchValueBridge`。它持有当前 committed `Signal<String>` 和生命周期 `Scope`：非受控从 defaultValue 初始化并由 Input.onChange 回写；受控从 value 初始化并订阅其 Prop，Input.onChange 只报告提议值，由 caller 的 authoritative 回写更新 bridge。Input 始终读取 bridge Signal，按钮读取同一 Signal；bridge 只由子控件回调及绑定弱引用维持，销毁子树后清理订阅。对回调自毁使用调用前本地强引用与一次性门禁，避免继续访问已卸载组件。

备选是 Search 自建 TextEditorState 或由按钮查找同窗当前 focused Input；前者违反复用目的，后者在多个 Search 和焦点切换时无法稳定关联。桥只保存 value 与提交策略，不复制编辑事务、selection、caret 或 IME。

### 2. Input 与 Button 分别保留原语义

`Search()` 用 `Flex` 的零 gap 横向布局，Input 子项 flex grow/shrink 且 min-width=0，Button 不收缩；整体 `LayoutStyle` 只作用于外部容器。Input 的 `onSubmit(String)` 已处理 Enter repeat 与 active composition，直接转入统一搜索门禁。Button 的 `onClick` 读取 bridge 当前 committed value；composition 临时串不写入 bridge，因此按钮不会把临时串当成已提交值。loading/disabled/readOnly 的当前 Prop 在两条路径提交时读取，并同步喂给 Button 的交互与 loading 视觉。Button 用既有 Default/Primary Token 对应 enterButton=false/true，默认显示可读“搜索”文字；自定义 typed slot 只替换内容，不绕开 Button 语义。Search 不制造第二个 focus target。

备选是复刻 Ant Design 的 DOM `Space.Compact`、搜索图标及输入边框拼接，既不能直接用于 C++ scene，也会迫使 Input/Button 公开视觉入口。首批采用可测的相邻控件与文字按钮，完整紧凑边角及图标样式留待有明确 Token/slot 合同的后续 change。

### 3. 保持独立 invalidation 与 generation 安全

Input 原有 value/placeholder 的 Text/Measure 路径、Button 原有 loading/pressed 的动画与 Material 路径保持独立；Search 不监听颜色并执行整体 remount。bridge 的订阅生命周期随两个子控件结束，旧回调不得访问复用 slot。Theme 快照对 Input 和 Button 分别解析；容器没有单独 Search 材质层。通过 retained ID、shape/measure、GPU upload 和 idle benchmark 验证这一边界。

### 4. 来源和分平台验收

离线来源锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，核对 `components/input/Search.tsx`、`components/input/style/search.ts`、`components/input/index.en-US.md` 与样例 SHA256。正式构建使用 `CMakePresets.json` 的 Ninja Multi-Config；平台通用合同在 Windows MSVC x64 上执行一次。Windows 单独检查 Win32、D3D12/DXIL、系统字体、实际显示缩放与多档模拟 scale；Linux GCC/Clang、原生 Wayland、Vulkan/SPIR-V 与 Fontconfig 单独保留 checklist，按用户安排暂缓。

## Risks / Trade-offs

- [组合状态与 Input 内部 controlled echo 时序不同步] → 回归覆盖外部 Signal 回写、无回写、程序化更新、非受控连续编辑和按钮提交；以 authoritative Signal 作为最终显示来源。
- [Enter 与按钮因 loading/disabled 动态变化产生双次提交] → 每次提交读取当前 Prop；分别测 key repeat、pointer cancel、loading 转换和回调同步销毁。
- [零 gap 下边框、焦点层或 hit-test 重叠] → 测三档尺寸、窄宽、四档模拟 scale 并做 Windows 实窗截图；必要的窄内部布局修复必须仍复用 Input/Button scene。
- [默认文字按钮与上游默认搜索图标不完全一致] → Gallery 明示 partial；不将首批结果称为完整 Ant Design Search 视觉等价。

## Migration Plan

只新增 Search API 与组合接线，不迁移 Input/Button 的公开接口。若 Search 阶段回归失败，可回退 Search 独立实现提交，保留 010/013/011 已验证基础；011、013 的 Linux 待验项保持原状态。
