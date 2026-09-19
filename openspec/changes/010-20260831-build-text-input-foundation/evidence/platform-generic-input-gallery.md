# 平台通用：Token Gallery Input live sample

2026-09-19；任务 8.2；Windows/MSVC，正式 `windows-msvc-debug` preset。

## 实现范围

- Token Gallery 的 Live Samples 增加受控与非受控 `ryn::Input`，展示中文、Latin、emoji、placeholder、prefix/suffix、scalar `maxLength`、响应式 status、`onChange` 与 `onSubmit`。
- 页面明确标注当前为 `partial`：支持单行 Input；未实现 `TextArea`、`Password`、`Search`、`OTP`、`allowClear`，且系统 IME/视觉仍需 Windows 与 Linux 分平台验收。
- Gallery runtime 将 committed text、composition、candidate、keyboard、window focus 与 display scale 路由到同一 `InputComponentHost`；最终 layout/scroll 同步后，使用独立的 render-logical 到 window-coordinate 比例更新候选窗 input area。
- `ReferenceSurfaceHost` 可在同一次 root mount 中同时激活 ReferenceSurface 与 Input host，保持一次性 Component mount 合同。
- 72 项支持 overlay 中只有 `ant.component.input` 从 `planned` 更新为 `partial`。离线生成器要求已完成 8.1、可解析的公开 API/runtime/evidence 文件及已注册的 Input journey、component、Gallery frame tests；同时拒绝把当前 Input 子集标为 `implemented`。

## 验证

`rynui.token_gallery_frame` 的 Input journey 覆盖 preedit 不触发 change、受控中文/emoji commit echo、submit、非受控多行 clipboard 归一化、input-area、window blur、session start/stop 配对与 idle。编辑前后 component/editor/text-layer identity、TextScene 数量、Quad/effect topology 及 root/Theme content run 保持稳定；文本长度变化允许更新 glyph draw range。

Gallery catalog generator 的 `--check --self-test` 通过。定向运行 Gallery catalog generator/contract、Input journey、ReferenceSurface runtime/contract、Token Gallery frame、reference catalog 与 document model 共 8/8 通过（21.29 秒）；`rynui.token_gallery_frame` 20.30 秒。

上述为平台通用 headless/CPU/fake GPU 证据，不代表真实窗口、驱动、系统中文 IME 候选窗或人工视觉已经通过。
