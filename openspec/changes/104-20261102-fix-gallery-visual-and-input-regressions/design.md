# Design

## Context

见 proposal.md。源码调查确认 Quad 的 nominal bounds 同时作为 draw bounds，未提交外侧 AA fragment；Table 后代填充转换依赖组件名单；RadioButton 相邻背景能覆盖选中边的 AA。文字行框与实际 ink、布局约束和旋转图标需按真实路径验证，不能把所有偏心归因于同一常量。

## Goals / Non-Goals

**Goals:** 修复本批 25 条反馈的实际行为，保留 Theme、typed Props/slots、retained 身份与局部更新。按共用根因修改、按组件场景验证。

**Non-Goals:** 不更换字体/渲染技术栈，不增加彩色 emoji 管线、Qt/qrc、组件专属 GPU 上传或新 backend；不宣称本机验证了 Linux 或所有 DPI/显示器。

## Decisions

- Quad 在 renderer 端区分 shape 与带物理 guard 的 draw bounds，顶点外延保持原形状大小、圆角及 48-byte instance ABI；AA 与 RoundedEffect 使用一致物理宽度。只调整 smoothstep 或扩大 shape 会分别遗留缺失 fragment 或改变布局。
- 普通填充按实际 content clip 转成现有可裁剪效果，取消组件名单作为正确性的前提；保持窗口层与 content/decoration 规则，转换退出必须清除旧命令。
- 共享选中边按绘制所有权合成一次，覆盖完整 AA 域；虚线按周长累计 phase，圆弧 rail 与进度共用端点。接缝统一几何覆盖，避免透明层叠加加深。
- 文字和 Icon 分别核对 linebox/baseline/ink。优先修复共享测量/放置契约，再修具体调用者的错误约束；不在 Gallery 字符串前后加空格或固定像素偏移。
- 系统 fallback 加入真实可光栅的候选并验证 Unicode shape；正文 emoji 保留，控件箭头仍用 bundled Icon。逻辑鼠标样式不暴露 SDL，平台宿主负责系统 cursor 资源生命周期。
- Switch 保留两套 retained 内容，按状态和手柄几何裁剪/放置，并验证动画中间态、反向与停稳；不通过重建掩盖位置错误。

## Risks / Trade-offs

- 真实裁剪会增加有 clip 的 effect 数量 → 仅有实际裁剪需求时转换，保留缓存及局部更新。
- 字体基线改变可能影响多个控件 → 覆盖中英、Icon、空值 caret 和混合 fallback，避免改变普通正文语义。
- 透明 seam、共享边和 fractional DPI → 用 source-over 与 draw bounds 回归，不能只测 SDF 中心点。
- 新 shader 内容需重新内嵌 → 使用正式 MSVC/Ninja Multi-Config preset 重建 shader/最终 Gallery，保留 RC 独立部署。
- native 产物被用户占用 → 先验证相关后台 targets，交付前检查进程，不任意关闭用户窗口。

## Migration Plan

按阶段构建受影响 target/CTest，保留原断言，独立 commit 后安全 fetch/rebase/push 并核对远端。失败回退本阶段即可；原始输出仅保留 out，仓库记录简短结论。平台通用验收使用 windows-msvc-headless Debug/Release；Windows 字体/鼠标/真实 GPU 使用 windows-msvc Release。Linux 原生复核独立记录待验，不代打勾。

