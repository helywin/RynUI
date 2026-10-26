# Astra full R1 四项 P2：真实复现与修复

父转交的 full R1 为非 CLEAN，四项均由源码推导。本 worker 在原隔离工作树、分支 `t3/add-native-rate`、干净生产基线 `cea1552c85fdcb206a877e2c95037ed3d78ee660` 上先逐项实际复现，再修复。没有导入 moving main `34eefb82`、其他组件/WIP，未访问远端或创建 children。本文是实施响应与本地证据，尚未接受 fresh Astra R2。

## Before

仅添加测试 harness 和 CMake 目标，生产实现保持 cea1552c；在当前隔离树新编译/链接 `rynui_rate_review_tests`，使用本树程序：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_review_tests
out/build/windows-msvc-headless/tests/Debug/rynui_rate_review_tests.exe geometry --before
out/build/windows-msvc-headless/tests/Debug/rynui_rate_review_tests.exe pointer_gap --before
out/build/windows-msvc-headless/tests/Debug/rynui_rate_review_tests.exe focus_exit --before
out/build/windows-msvc-headless/tests/Debug/rynui_rate_review_tests.exe keyboard --before
```

实际日志：

```text
BEFORE geometry: popup=1 focused=1 session=1 preedit=1
BEFORE pointer_gap: popup at (5,21)=0 popup_top=32
BEFORE focus_exit: popup after Rate->Input->Button=1
BEFORE keyboard: value=2.5 focus_visible=0 outline_coverage=0 emphasis_width=20
```

四项全部复现，无与推演不符的项。几何测试先实际打开富 Tooltip、聚焦 Input 并建立 composition，再将 width/height 设为 0；焦点路径由实际 FocusManager Tab dispatch 驱动。outline coverage 取实际 logical RoundedEffectStore 实例采样，不用 snapshot bool 代替。

## 最小实施响应

1. Tooltip 可见和 owned input block 纳入 `geometry_active`；在几何边界变化时通过既有关闭/边界路径取消后代 focus/session/preedit。根仍参与布局以便 0 尺寸恢复，不用隐藏 root 造成无法恢复。关闭回调后以完整 ComponentId 重新查询。
2. 组件 State 仅增加一个可取消的 100ms leave deadline，使用现有 AnimationTime、非阻塞 tick 与 next_auxiliary_deadline。进入项/浮层取消，越过间隙保留，过期清除；隐藏、无效几何、输入阻断及卸载取消。deadline 保存在完整 generation 所有的 State 中，无全局 timer/机制，也不为无 Tooltip 的 Rate 创建空 deadline。
3. 使用既有 prepare_auxiliary_layout，在实际焦点 identity 变化时协调后代退出。原生 popup 内合法焦点仍保持，readOnly 不关闭合法原生输入容器；纯 Tab 离开后关闭。
4. 通过既有 defer_focus 切换 keyboard modality。pointer modality 转换中的命令保留为单个本地 pending key，由焦点回调刷新后按完整 generation 重新查询并消费；已为 keyboard 时安全消费一次。回调卸载/重绑后不得编辑旧或新模型，keyup/activation repeat 不追加提交。

未改字号 float/round 原子性校验、FontRuntime、共享全 window hit + 普通 document clip、renderer ABI、Theme token/identity、其他组件或 Gallery。

## After：实际新回归与直接相关代表

- geometry：断言 popup/focus/session/preedit 全部取消，旧 session 拒绝；恢复后不自行复活 popup，再进入保留 item/editor/draft/ref 与只挂载一次的富 slot；清理无 owned blocks 残留。
- pointer_gap：从 `(5,8)` 开始逐像素、逐单调时间步跨越 20..32 间隙，再连续进入 Input，实际鼠标点击聚焦。间隙中切换 readOnly 仍可到达子控件。进入取消 deadline，离开过期关闭，随后 idle 无 frame/deadline；带未到期 deadline 卸载并替换 generation 后，旧时刻 tick 不影响新模型或 ref。
- focus_exit：真实 Tab Rate→Input→Button 关闭；重新 Tab 进入、切换 readOnly，合法 Input 焦点保留，再 Tab 退出关闭。
- keyboard：从 pointer 分别进入 Right/Home/End 后，断言 focus_visible、实际 outline coverage > .1、背景 emphasis 宽 22；Right 每个 repeat 只走一步，keyup 无提交，activation repeat/keyup 无追加。焦点回调同步销毁旧 Rate、创建替代 Rate 并重绑 InputRef，断言替代值仍为 4、旧命令无 onChange、新 ref 可用。
- 既有 rate_component、rate_lifecycle、rate_font_limit 原断言全部保留并复验，覆盖原受控拒绝、clip、原生内容/资源与字号上限；不重复无关 full matrix 或金样生成。

官方 MSVC 19.51 `windows-msvc-headless` / Ninja Multi-Config，最多 2 jobs，只运行隔离树新编译/链接的程序：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_review_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests
ctest --preset windows-msvc-headless-debug -R '^rynui\.(rate_r1_|rate_component$|rate_lifecycle$|rate_font_limit$)' --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_rate_review_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests
ctest --preset windows-msvc-headless-release -R '^rynui\.(rate_r1_|rate_component$|rate_lifecycle$|rate_font_limit$)' --output-on-failure
```

最终 Debug：7/7，2.53 秒；Release：7/7，1.61 秒。均为 HEADLESS 原生运行时/逻辑场景证据，不代表 native window/GPU/Linux/系统 IME/AT 验收。

收口检查使用 clang-format 22.1.3、父给定 explicit OpenSpec 1.14 的 doctor/strict、diff check；对 cea1552c 核对字号校验、共享 clip 和五金样未改动。完成本地英文提交后停待父 fresh Astra R2，不自行 merge/push/PR/archive。

实际收口结果：739 个自有源文件格式检查 0 failures；doctor healthy=true；strict 88 passed / 0 failed；working/index diff check 均通过。`git diff --exit-code cea1552c85fdcb206a877e2c95037ed3d78ee660 -- src/theme/rate_theme.cpp src/component/window_component_services.cpp design-tokens/ant-design/6.6.5/golden` 返回 0，无需重跑金样矩阵。
