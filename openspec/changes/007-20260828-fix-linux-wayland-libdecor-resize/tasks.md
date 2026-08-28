## 1. 锁定 patched libdecor 输入

- [ ] 1.1 在集中 dependency lock 与 license 清单中固定 libdecor 0.2.5 release URL、SHA256、MIT license、上游 resize-state commit identity 和两份 patch hash；运行 lock/license schema tests，证明不存在 moving branch、submodule 或未校验输入
- [ ] 1.2 添加上游 `XDG_TOPLEVEL_STATE_RESIZING` 四行 patch 及最小 configuration retain/release patch；用隔离源码 fixture 验证两份 patch 按序成功、重复或错误版本 fail-fast，且没有修改 libdecor version
- [ ] 1.3 以 CMake `ExternalProject` 编排 build-local Meson libdecor core/cairo plugin，显式检查 Wayland、wayland-scanner、cairo/pangocairo 和 Meson/Ninja 平台服务；通过缺工具/缺 package configure-fail fixtures 与 target-order contract 验证诊断和依赖顺序
- [ ] 1.4 建立内部 canonical libdecor target、staging include/lib/plugin path 与 build RPATH，验证 Linux `BUNDLED` 不加载系统 libdecor、Linux `SYSTEM` 不回退下载、非 Linux分支不创建 libdecor target；运行 dependency tests 和 `git diff --check` 后以英文 `build: add patched bundled libdecor` 提交本阶段

## 2. 在 SDL 配置前应用 feature patch

- [ ] 2.1 为锁定的 SDL3 3.4.14 添加严格 patch step，使 patch 在 `FetchContent_MakeAvailable(SDL3)` 配置前完成；通过 pristine archive fixture 验证正常应用、错误上下文失败且 source override 仍使用同一 patch
- [ ] 2.2 扩展 SDL bundled Wayland CMake 检测以消费 build-local libdecor target/header并定义私有 resize-state feature，关闭对任意系统 libdecor 的动态探测；通过 generated build graph 和 link/runtime-path contract 验证只连接 staged library
- [ ] 2.3 修改 `decoration_frame_configure()`，仅对 `LIBDECOR_WINDOW_STATE_RESIZING` 读取去掉 `SDL_LIBDECOR_CHECK_VERSION(0, 3, 0)` 限制；用 source contract 证明 resize bit 在 bundled feature 下启用、其他 bounds/wm-capabilities 0.3 gates 未变化且没有伪造版本号
- [ ] 2.4 使用 `linux-gcc` fresh configure 构建 patched SDL/libdecor 最小目标，核对编译 header、ELF dependency、loaded library 与 plugin 均来自 staging prefix；运行 SDL patch/dependency tests 和 `git diff --check` 后以英文 `fix: propagate libdecor resize state` 提交本阶段

## 3. 修复 configure/ack/frame 前进状态机

- [ ] 3.1 在 patched libdecor 中实现 configuration retain/release 引用所有权并覆盖 callback 返回、额外 client 引用、最终释放及异常销毁测试，证明 serial/state/size 内容不变且无 double-free
- [ ] 3.2 在 SDL libdecor window data 中保存至多一个 pending interactive-resize configuration：收到新 configure 时替换旧引用、首次 pending 请求 exposure、frame callback 确认最新 serial，resize-clear 即时提交，隐藏/销毁/断开统一释放
- [ ] 3.3 添加可注入状态机测试，覆盖 configure burst 合并、serial 单调 ack、同 serial 不重复、exposure→present→callback→ack、resize 结束、focus lost/gained 不参与前进、窗口中途销毁和 outstanding reference 恒定容量
- [ ] 3.4 运行 `rynui_minimal` Wayland smoke 并记录 received/replaced/acked/frame 诊断，证明拖动期间持续更新而无需失焦，随后运行受影响 CTest 与 `git diff --check`；以英文 `fix: pace libdecor resize commits` 提交本阶段

## 4. Linux 构建与自动回归

- [ ] 4.1 使用 `cmake --fresh --preset linux-gcc` 完成 Debug/Release build 与受影响 CTest，核对 Ninja Multi-Config、标准 C++20、patched libdecor/SDL 来源、RPATH/plugin、public dependency isolation 与正常退出；保存实际 preset 和测试计数
- [ ] 4.2 使用 `cmake --fresh --preset linux-clang` 完成 Debug build 与受影响 CTest，核对 ExternalProject 生成物可由 Clang consumer 链接且 SDL/libdecor C ABI 无 toolchain 泄漏；不重复运行平台通用完整 CTest
- [ ] 4.3 运行隔离的 `BUNDLED|SYSTEM` positive/negative contracts、patch source contract、license、未跟踪依赖和 public-header leak checks，证明 SYSTEM 不回退且公开 API 不出现 SDL/libdecor 类型
- [ ] 4.4 汇总自动验证结果并更新 build evidence，运行 `openspec validate --all --strict --no-interactive` 与 `git diff --check`；以英文 `test: validate patched Wayland dependencies` 提交本阶段

## 5. Linux 原生 Wayland 真实窗口验收

- [ ] 5.1 记录本机 `XDG_SESSION_TYPE`、SDL video driver、GNOME/Mutter、GPU/driver、两个输出的 mode/refresh/scale、实际 loaded libdecor/plugin path 和 patch identity，确认后续测试未设置 `SDL_VIDEO_DRIVER=x11`
- [ ] 5.2 在 240 Hz、display scale 1.333 输出快速连续调整 `rynui_minimal` 与公开 layout/token 示例，保存拖动期间连续帧证据、configure/replace/ack/frame 计数和退出码；人工确认不等待失焦、pointer 命中与 logical viewport 对应
- [ ] 5.3 在 60 Hz、display scale 1.0 输出重复相同操作并保存证据；人工确认拖动期间持续跟随且松开后没有继续追赶旧尺寸的明显动画队列
- [ ] 5.4 把窗口从两个输出之间双向移动后立即 resize，验证 drawable、logical viewport、display scale、字体 raster 和 pointer 坐标使用当前输出指标，并正常关闭窗口
- [ ] 5.5 更新 `docs/issues/linux-wayland-resize.md` 和受影响 change evidence，只在 5.2–5.4 全部通过后将问题标记解决；运行 evidence contracts、OpenSpec strict validation 与 `git diff --check` 后以英文 `test: validate native Wayland resize` 提交 Linux 验收阶段

## 6. Windows 依赖隔离验收

- [ ] 6.1 在实际 Windows/MSVC 环境使用 `cmake --fresh --preset windows-msvc` 完成 Debug build 与依赖隔离测试，证明 configure/build graph 不下载、不构建也不链接 libdecor；记录 preset、MSVC x64、退出码和测试计数
- [ ] 6.2 运行 Windows dependency evidence contract、public-header isolation 与 `git diff --check`，更新独立 Windows evidence并以英文 `test: validate Windows dependency isolation` 提交；不得用 Linux静态合同代替本 checkbox

## 7. 变更收口

- [ ] 7.1 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、完整平台通用 CTest（若 4.1 已完成同一提交状态则复用其结果）和 `git diff --check`，确认所有 lock、license、patch、源文件与证据均已跟踪且工作树不含临时诊断修改
- [ ] 7.2 汇总 Linux 与 Windows 独立证据、commit IDs、剩余限制和上游迁移条件；只有本 change 所有 checkbox 都有对应验证后才报告实施完成，不主动 push 或 archive
