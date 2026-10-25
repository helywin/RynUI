# Astra R1 修复证据

## 范围与失败回归

纯基于 `4545748c5a870c42e1d43084cec79ba0e8a9403b` 的隔离树，分支 `t3/native-timeline`。只修复 Astra R1 的 3 项 P2；原首版证据是历史 headless 结果，不等于 R1 CLEAN。相关 2.1/2.2/3.1/3.2 已先重开，本轮没有新增功能/change/child/PR/archive，也没有访问 Git 远端或导入 main/085 WIP。

先新增并编译 `timeline_review_tests.cpp` 的 fill/loading/capacity 三个 CTest，然后在未修改 production code 上实际运行：**0/3**，全部按预期失败。

1. `ordinary document Primary Button fill crossed Timeline region`。64×32 Timeline，Title 约 11dp，Primary Button 自然宽度超过 64dp；检查最终 ordered commands 中的 quad / rounded-effect 与 shader clip 后真实 coverage，不以 glyph/hit/snapshot 代替 fill。fixture 用 Gray 节点避免将同色节点描边误算成 Button 背景。
2. `dash actual=1 expected=4`，A 普通、B.loading，正序把应为 4 个 dash 的 rail 画成 1 条实线。
3. `retained content visual count is invalid`，两个 loading、padding 40000dp、600×50000 viewport/clip，合法 Theme 超过单 range 4096 上限。

## 逐项修复与证据

### P2-1 普通 flow fill

WindowComponentServices 把 Timeline mounted roots 与 document clip 传给既有 retained clipped-fill pass。只有这些普通 document 子树新增准入，不把 Timeline 标记为 window layer，也不缩小真实控件。fill 使用 region content clip，focus/effects 独立使用 decoration clip 并服从外部 hardclip。无额外 clip 的 normal Primary Button 仍发布原 quad。

surface 与 content range 的旧 clipped-fill IDs 在失活或退出准入时清理；surface 装饰恢复生产者原 effects。即使 enable=false，仍走转换缓存收尾，避免上一次转换冻结后续 geometry/material。

最终 fill 回归通过，原 region_clip 的实际 R8 ink、真实控件自然尺寸、外扩 focus coverage 和 hardclip 回归也通过。

### P2-2 loading 邻接 rail

正序取下一显示项 loading，反序取当前项 loading。实际 ordered draw commands 的 dash 数量/端点为 **4/4**，正序、反序和隐藏恢复均通过；单独剩下的最后项无 tail。

### P2-3 多 range 长轨迹

dot 保持原 retained surface，tail 按每 range 最多 4096 quads 分批持有 component-owned fragments。轨迹可见长度与相位保持完整；缩小 clip 时销毁多余 ranges，扩大后恢复，反序/隐藏/零宽/销毁均清理对应资源。

实际 ordered commands：完整轨迹 **5001/5001**，缩小 clip 后 **11/11**，恢复、反序、隐藏恢复和零宽恢复后均 **5001/5001**。最终 teardown 的 retained surface 与 rounded-effect live count 均为 0。可见工作量在循环前计算，用有界整数循环；0 tail width 不生成 range，超出 logical scene 32-bit 索引可表示数量显式 length_error，不静默截断也不扩大 ABI/4096 上限。

## 验证

实际 Windows MSVC 19.51，`windows-msvc-headless` / Ninja Multi-Config，每次 build 最多 4 jobs。

- Debug：**15/15**，13.92 秒。
- Release：**15/15**，8.29 秒。
- 同一选择包含原 7 项 Timeline、3 项 R1 回归，以及 descriptions_component、portable.default_theme/button_scene_service/select_component/modal_boundary 五个必要共享代表。

```text
ctest --preset windows-msvc-headless-debug -R '^rynui\.(timeline_.*|descriptions_component|portable\.(select_component|modal_boundary|button_scene_service|default_theme))$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.(timeline_.*|descriptions_component|portable\.(select_component|modal_boundary|button_scene_service|default_theme))$' --output-on-failure
```

旧五份 typed golden 值、33 IIFE 原文/顺序、旧 TokenIdentity ordinals 再次深比较保持；本轮未改 Theme/Time/default config/Binding bridge/switch publish_theme/PE stack。GPU/native/Linux/系统 IME/AT 未验收，父负责独立 R2 与最终组合发布。

clang-format 22.1.3 `--check` 检查 692 个自有源文件、0 failures；显式调用 `C:/Users/jiang/AppData/Local/pnpm/bin/openspec.ps1`（1.14.0），doctor 健康，strict validate **84/84**，git diff --check 通过。工作仅本地，不把这组 headless 验证描述为独立 Astra R2 CLEAN。

## 与 085 合并的具体并置点

没有读取或导入 085。RetainedSurfaceService 两个准入点当前保留 `document || (clip_window_layers && in_window_layer_stack(component))`：父合入 085 的 stack→in_window_layer 时，保留 `document ||` 与 Timeline roots/document clip 参数。WindowServices 保留父的 window-layer enable 表达式，同时继续传 Timeline roots 和 document clip。不能恢复 enable=false 时短路整次 synchronize_clipped_fills；surface/content 两类失活与转换缓存均需要收尾。085 owner 的同类清理应逐处合并，不能整文件覆盖。
