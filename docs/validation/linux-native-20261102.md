# Linux 原生验收（2026-11-02）

实际环境：Arch Linux x86_64、GNOME 原生 Wayland、SDL Vulkan/SPIR-V、Fontconfig 系统字体；GCC 16.2.1、Clang 23.1.3、CMake 4.4.4、Ninja 1.13.2。正式 preset 使用 Ninja Multi-Config 和锁定 BUNDLED 依赖。

## 构建与平台回归

- `cmake --fresh --preset linux-gcc`、`cmake --fresh --preset linux-clang`：通过。
- `cmake --build --preset linux-gcc-debug`、`linux-gcc-release`、`linux-clang-debug`：通过。
- GCC Debug 受影响回归：`ctest --preset linux-gcc-debug -R '(hit_test|default_font_chain|notice_theme|timeline_theme|descriptions_theme|shadercross_override|animation_architecture|layout_demo|space_public)' --output-on-failure`，13/13 通过。
- Clang Debug 字体、平台输入、生命周期、shader、资源、依赖、hit-test、受影响 Theme/布局合同：56/56 通过。
- 最终扩大后的 Clang 定向回归：65/65 通过（排除重复的完整 Gallery journey）。GCC 完整 CTest 曾为 553/555；两个 Gallery 项目的旧 inventory / 导航 / 清理断言已修复，相关 Input、Empty/Result、Details/Loading、Overlay/Timeline、Member/Metrics、Tree/AutoComplete、TreeSelect 独立案例全部通过。后续完整运行按用户要求终止，不记录为 555/555。
- 修复 Linux 无法编译的共享字体角色枚举、fixture 相对 include；Fontconfig coverage 请求及字重单位；三个 Theme 的字体上限使用真实栅格器范围，避免依赖 Linux/Windows 不同的 `long` 宽度。
- 配置夹具补齐 SheenBidi，Space 负编译与工作流合同对齐当前 API/AGENTS；布局测试等待完整有限时长的 Button wave。

本节只记录构建及自动测试，不代表系统 IME、人工视觉或跨实际输出缩放验收。原始 log、GPU readback、CSV/JSON 等仅保留于忽略目录 `out/linux-validation/20261102/`。

## 长页面滚轮延迟

Astra xhigh 分析定位：一次 Gallery 滚轮路由会查询同一点约 15 次；原 hit-test 对几何落空的记录也先遍历 blocked roots / 组件祖先。滚动条拖动绕过组件滚轮路由，两者后续共用 scene/render 提交路径。

修复仅将 clip/bounds 的落空判断提前到 branch activity 检查之前，保留 stale generation 诊断、实际候选的动态祖先/屏蔽/输入边界判断，不缓存跨事件目标。回归覆盖动态失活/解除屏蔽、边界和 stale 记录；密集 benchmark 使用 1,000 个记录、8 层祖先、200 个 blocked roots、10,000 次查询：10,000,000 条记录检查，branch checks=0，allocations=0（Release 171,302 µs，总查询时间，不是单个滚轮时间）。

`SDL_VIDEODRIVER=wayland SDL_GPU_DRIVER=vulkan .../Release/rynui_token_gallery --scroll-acceptance` 两次通过：每次实际分发 240 次滚轮、3,600 次 hit queries，branch checks=0；平均输入分发 320.934 / 310.569 µs，最大 1,313.95 / 482.283 µs。完成到底部 clamp，content runs=1，translation-only 阶段 reconciliation / anchor refresh 均为 0。这里测的是输入分发，不是 GPU 帧耗时；不与不同 viewport 的修复前调试测量计算倍数。

`--scrollbar-acceptance` 通过：连续拖动、头部导航、侧栏 wheel/drag、文档 track/drag 均通过。验收使用程序内部事件注入，过滤外部输入、等待初始配置/锚点稳定后计时；没有操作桌面鼠标或键盘。用户实际鼠标手感仍由用户确认。

## 原生组件与 GPU 读回

在用户要求后台运行之前，GCC Debug/Release 均已完成 23 种原生验收模式 × 4 档 render scale（1/1.25/1.5/2），各 92/92 通过，覆盖 Typography、三类 Slider、两类 Tooltip、Button/Divider、Switch/Checkbox/Radio、Flex/Space/Icon、Input/TextArea/OTP/Bidi、Rounded、Select/Card/Tabs/Modal。它们使用程序内部事件，render scale override 不等于跨实际显示器验收。

Rounded/Switch/Radio 另含 system scale，共 10 组 GPU 读回：`python tools/verify_native_gpu_feedback.py out/linux-validation/20261102/native --work-dir out/linux-validation/20261102/gpu-pixel-checks` 通过。440 个圆角区域 / 6,485,688 像素，560 个 Switch 中间帧，80 个 Radio 案例 / 911,864 像素；最大通道误差 1/255，容差 2/255。Rounded resized BMP 均为 1420×1000。

夹具修复包括：Wayland 最大化窗口先 restore、同步并确认实际尺寸；圆角 reference 使用最终 drawable extent；Tooltip 命中实际 trigger；有限 Button wave 完成后检查 idle；Typography 无真实 strong/italic face 时验证明确诊断及 regular fallback。新增尺寸确认后仅增量构建相关 Gallery target，不重复整套矩阵。

## 后台运行边界

用户要求不再弹出前台窗口后，停止前台矩阵与重复全量 CTest。后续默认命令：

```sh
python tools/run_background_wayland.py --log-dir out/background-smoke -- \
  out/build/linux-gcc/examples/Release/rynui_minimal --smoke
```

工具仅在独立 headless Mutter、私有 Wayland socket / D-Bus / runtime 下创建窗口，使用内存 GSettings 与本地 GIO，不连接当前桌面；超时退出并清理，不回退前台。实际后台 minimal Vulkan/SPIR-V smoke exit 0。动画 readback 在本机 headless compositor 的 swapchain 等待超时，后台动画矩阵未通过，保留此限制，不修改 renderer 掩盖它；此前原生 GPU 结果不由后台 smoke 代替。

系统 IME 候选窗、跨实际显示器/DPI、人工视觉与鼠标手感仍待用户安排。Windows 清单保持原状态，不自动 archive。

本轮共更新 44 个 change 的 47 个已验任务；各任务链接到本摘要。change 100 的 3.2 单独保留为用户手动确认滚轮流畅度，当前未勾选。未通过或尚未进行的项目保持未勾选，不因同一 change 的其他测试通过而关闭。
