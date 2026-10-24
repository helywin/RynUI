# Astra R1 两项 P2：复现与修复

## Scope

从 `6dd6f415814293a830435c4a4f2481e42a22f98a` 开始，只修改本 worktree 的 Breadcrumb/Steps host、相关测试、API 文档与 080 tasks/evidence。收到 R1 的两项 P2 后，将 tasks 2.1/3.1/4.1 重新打开；本阶段 focused checks 完成后仍保持打开，等待组合验收。没有导入父线程或其他组件 WIP，没有改 Core、InteractionRegistry、renderer/backend 或 SDL 边界。

## BEFORE：先执行失败复现

先新增 `tests/navigation_review_tests.cpp`，生产 host 仍是 6dd6f41，`git diff -- src/component/breadcrumb_component.cpp src/component/steps_component.cpp` 为空。实际运行：

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_navigation_review_tests --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui.navigation_review_' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_navigation_review_tests --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui.navigation_review_' --output-on-failure
```

Debug/Release 均 **0/5 passed，5/5 failed**：

| Case | 实际失败 |
| --- | --- |
| children_steps | 无 onChange，native Input 的真实 HitTest expected=1 / actual=0 |
| children_breadcrumb_current | 有 onActivate，但为当前项，native Input HitTest actual=0 |
| children_breadcrumb_display | 无 onActivate，native Input HitTest actual=0 |
| focus_steps | 单步骤已 Tab 聚焦，四条外扩边缘 shader/clip coverage 为 0,0,0,0 |
| focus_breadcrumb | 单祖先已 Tab 聚焦，四条外扩边缘 coverage 为 0,0,1,0 |

完整本地原始日志位于 ignored `out/080-r1-repro-{debug,release}.log`；失败后才修改两个生产 host。

## Changes

- interaction 的 eligible 表示可继承的输入资格，仅由真实 visible/disabled 控制；导航资格通过自身 focusable、Tab stop 与既有激活门控控制。current/无导航 callback 不再禁用整支富内容输入。
- disabled、hidden、Modal/Spin 仍屏蔽整支输入，并清理子 Input 的 focus/composition/session。窗口失活时通过组件自身输入边界屏蔽行分支，恢复时重新开放；未改通用 Input 或全局窗口输入实现。
- 焦点 outline 使用 `component_clip(id, ...)` 已计算的外部祖先 clip，背景/状态圆/连接线/slots/glyph/HitTest 保留严格 content clip。零宽或零高不绘制 outline。
- 更新旧 Steps 展示态测试，断言导航自身不可 focus/Tab，而非把输入父 interaction 判为全支 ineligible。API 文档说明展示/当前项中的原生输入资格与外扩焦点裁剪合同，C++ 示例不变。

## AFTER：focused acceptance

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_navigation_review_tests rynui_steps_component_tests rynui_breadcrumb_component_tests --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui.(navigation_review_|steps_component$|breadcrumb_component$)' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_navigation_review_tests rynui_steps_component_tests rynui_breadcrumb_component_tests --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui.(navigation_review_|steps_component$|breadcrumb_component$)' --output-on-failure
```

Windows / MSVC 19.51 / Ninja Multi-Config / windows-msvc-headless，build 并发上限 4。最新结果：Debug **7/7**（2.31 秒），Release **7/7**（1.45 秒）。

- 三个非导航富内容场景执行真实 Input/Button HitTest、pointer click、Focus、TextCommitted、composition/Enter、Button keyboard activation，且不发额外导航请求。Breadcrumb 祖先→current 保留子 Input 会话；disabled/visible、Modal、Spin、window active 往返恢复身份与资格。
- 真实 disabled 下子 Input 不可程序聚焦，composition/session 关闭；子 Button 与 disabled 导航均不能激活。window/Modal/Spin 阻挡后只恢复有效内容。
- Steps 单项/多项、H/V；Breadcrumb 单祖先/多祖先；正常、短、零分配，以及真实外部 ancestor clip/document clip。以 packed effect 的实际 outline coverage 加显式 shader clip 判断四条外扩边缘，有空间的非空项均为 **1,1,1,1**，外部祖先裁掉的左/上为 0 且右/下保持 1，document clip 与空分配下全为 0。
- 同时验证实际 glyph/icon layers、非 focus 表面与命中仍位于严格内容范围；子 Button 的 positive shadow 保留且受内容祖先 clip 约束。
- 原 Breadcrumb/Steps component tests 继续覆盖 controlled 拒绝/重试、原始索引、隐藏重排、retained slots、回调销毁及空闲帧。

本阶段不声称 Astra R2、Gallery、原生窗口/GPU、Linux、系统 IME 或辅助技术验收；composition 是 headless 注入输入。组合检查和 fresh golden 校验由后续证据记录。
