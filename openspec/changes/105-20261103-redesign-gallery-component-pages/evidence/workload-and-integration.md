# 通用工作量与最终集成

2026-11-03，实际 Linux / GCC / Ninja Multi-Config / BUNDLED。通用实现采用 `linux-gcc-headless`（HEADLESS + RECORDING）；原生 Gallery 采用 `linux-gcc` Release。

- 相关最终 target 构建通过。分批定向 CTest，按测试名合并修正后的最新结果：Release **14/14**，Debug **8/8**；没有执行全量 CTest。包含页面模型/viewport、宿主/生命周期/真实样例、frame contract、catalog/source generator、finite 参数/runner/schema 和 portable backend boundary。
- `rynui_gallery_pages_benchmark` 走与原生 `GalleryPageEvents::inject_scroll_at` 共用的 `GalleryPagesHost::inject_scroll_at → dispatch → InputComponentHost` 路径，未用直接 scroll_by 代替 wheel。登记视图从 2 组增加到 67 组；活动 Button 页的活节点数与文本遍历量相同，未访问页 factory 为零。32 个 fractional/反向 wheel 保持同一页面身份，8 次往返不增加活资源，blur 后无新增上传/期限，shutdown 的活资源为零。
- 输入、实际 layout、其余 host sync、SceneResources upload 与 Recording cull/submit 分别计时。Recording 是数据与排序检查，不是 GPU 性能或桌面延迟；没有 FPS/改善百分比门槛。逐项时间只在 out。
- HEADLESS 共用 Gallery 测试注册，修复其原先在 SDL_GPU 分支之后被跳过的问题；ReferenceSurface 实现与样例定义放在同一 CPU archive，避免 metadata 消费者的反向静态链接。实际 configure/build 与 portable guard 确认无 SDL/系统字体/具体 renderer 传递到 Core/common。
- GCC 16 的默认 C23 可满足 c_std_17，CMake 不一定生成 -std；guard 改为探测实际 C 编译调用的 __STDC_VERSION__ 与标准原子支持，保留 MSVC C17 atomics 检查。
- 新 UI 额外覆盖顶栏主题/Compact/Popover 键盘、过滤后的空结果重排、具名滚动条 hover 唤醒，以及失败页的短 extent、恢复按钮焦点和稳定 idle。文档卡默认 24 logical units 内边距，在 Compact/紧凑宽度使用 16，宽度与内边距原位更新。
- `--page-acceptance` 提供明确页/家族/样例、resize/主题/弹层/输入退役有限流程；JSON 只写仓库 out。runner 的 Linux 路径固定私有 Mutter/Wayland，处理超时/退出码、拒绝过期结果、校验 schema 与实际平台/preset/backend；5 个 runner 单元用例包含实际 C++ JSON writer，但均不替代 native 运行。

必要入口：`cmake --build --preset linux-gcc-headless-release --target rynui_gallery_pages_benchmark rynui_gallery_pages_host_tests rynui_gallery_page_acceptance_tests`；`ctest --preset linux-gcc-headless-release -R '^rynui.gallery_pages_benchmark$' --output-on-failure`。Debug 使用对应 headless-debug preset。其余相关 target/CTest 由本 change 各精简结果分组列明；原始输出只在 `out/105-implementation/20261103/`。

最终格式检查：clang-format 22.1.3，845 个自有源文件，0 failures。原生 binary 的混用参数、未知页和 out 外输出，均在创建窗口前以 exit 1 拒绝，3/3 通过。

以上为 CPU/模拟文本平台、Recording 和 Linux 原生编译证据；Windows/MSVC、实际 GPU 呈现、系统 IME/真实显示器未由这些结果证明。
