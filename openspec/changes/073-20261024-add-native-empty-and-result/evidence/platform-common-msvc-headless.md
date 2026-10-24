# 073 平台通用验收与交接

日期：2026-10-24。基线：`5cb9f599d2bb0bce01a0a601244afa677a104a15`。本工作树未合并父任务的 Collapse、Segmented 或共享 Theme 栈帧修复。

## 环境与命令

- Windows，Visual Studio 2026 Community，MSVC 19.51.36256.0，Ninja Multi-Config。
- 官方 configure preset：`windows-msvc-headless`；独立 binary dir：`out/build/empty-result-headless`。
- BUNDLED 依赖模式；FreeType/HarfBuzz/SheenBidi/utf8proc 使用官方支持的 FETCHCONTENT_SOURCE_DIR 覆盖，源码来自 `D:/code/RynUI/out/build/windows-msvc-headless/_deps/*-src`。依赖编译产物保留在本任务自己的 binary dir。
- 构建命令使用 `cmake --build out/build/empty-result-headless --config Debug|Release --parallel 4`。
- 完整检查：`ctest --test-dir out/build/empty-result-headless -C Debug|Release --output-on-failure -j 4`。
- 专项检查：`ctest --test-dir out/build/empty-result-headless -C Debug -R "empty_result|theme_algorithm|switch_features" --output-on-failure -j 4`。

## 已通过的本组件证据

Debug/Release 的 Empty/Result public API、Theme、component 与 Theme algorithm 四项均通过。最终 Debug 连同 Switch features 为 **5/5**，2.92 秒；最终 Release **完整 142/142**，15.12 秒。

专项包含：

- typed Props/slots、reactive Prop<T>，Status 复用和 403/404/500 映射。
- 默认 Empty 100 dp、紧凑 40 dp、Result 72 dp 的真实 Icon glyph 与 node 尺寸。
- 有限宽度的文本块居中/换行和 extra 操作间距/换行。
- 实际 node/text/glyph/surface 在 76×70、12×5、0×0 与恢复到正常空间时的几何和可见覆盖；无负尺寸、非有限值或超出分配区域的可见 glyph/Icon/surface。
- 默认焦点 outline 在 extra 预留空间内仍有实际覆盖，未被内容 clip 裁掉。
- Button 鼠标激活、Input 提交文本；普通 label/status/image/Theme 更新后保持 slot 调用次数、component/interaction/editor 身份、编辑值、焦点和输入会话。
- Theme 颜色只改变 glyph material、不增加组件测量；metric 更新触发测量；暗色/紧凑/继承/局部算法/override/equality/hash/dirty phase 与非法配置原子拒绝。
- typed image/icon/content slots、空 label/image 分支、静态空闲无帧请求及完整销毁资源释放。

## 锁定参考与 golden

参考 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，本地源码的 style SHA256 与 sources.lock.yaml 一致：

- Empty：`1e42162fbe8fb98f69fedf6b334a51d2fb13e1a65fb0ba4f45c3d17b489e935f`。
- Result：`040124b7781009d4cc024877c65b0183b8dca64e463fc3f4747514a3f70bc98a`。

五份 golden 均由本构建树新生成的 `tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 写入。与基线 5cb9f59 逐层、逐数组比较，忽略 identity 后，旧字段全部保持：

- default：2,495 个叶子值。
- dark：2,495 个叶子值。
- compact：2,495 个叶子值。
- dark-compact：2,496 个叶子值。
- compact-dark：2,496 个叶子值。

合计 **12,477** 个旧叶子值完全一致，仅新增 empty/result 两个顶层对象；最终再次生成未产生 Git 差异。Release 的 golden 合同亦通过。

## Debug 全量未收口项

第一次完整 Debug 为 **138/142**，151.24 秒；不能将其描述为全量通过。

1. Switch features、Button component、Popconfirm theme 初次异常退出。CDB 确认 Switch 为 `0xC00000FD`，调用栈经过 ThemeSnapshot 构造和 resolve_theme。仅把 073 新增的 Empty/Result 两处局部 seed/map/alias 派生改为立即调用的引用捕获 lambda 后，Switch features 已通过。Button component 与 Popconfirm theme 的复验仍失败，CDB 均确认 `0xC00000FD`，调用栈仍经过 resolve_theme/ThemeScope。未修改父任务负责的 24 个既有派生块，也未修改公开 API 或线程栈上限。
2. Input scene allocation：256 inputs、20,000 updates，dispatch allocations=0、sync allocations=20,000。CDB 在 tracking 开启后的第一次 new 处定位：

   `std::allocator<std::_Container_proxy>::allocate → std::vector<ComponentId> copy → PopconfirmComponentHost::position_window_layers → WindowComponentServices::layout_and_synchronize`

   对应既有 `src/component/popconfirm_component.cpp:619` 的 `const auto ids = mounted_;`，即空 vector 的 MSVC Debug iterator proxy 分配。该源文件相对 5cb9f59 没有改动；本次没有修改它。Release 此项通过。

共享 Debug 栈修复和 Popconfirm 分配修复由父任务集成处理；之后重跑对应失败项及最终 Debug 收口。任务 4.1 保持未勾选。已通过的 073 平台通用功能、专项与 Release 证据不回退，也不把剩余问题归为 GPU/Linux/IME 未验收。

## 其余验证与边界

- clang-format 22.1.3：`python scripts/format-code.py --check`，568 个自有文件通过。
- OpenSpec doctor 健康；严格全量验证 **71/71**；`git diff --check` 通过。
- Gallery、support-overlay、README EN/ZH、component-completion 未改动；不 push、不建 PR、不 archive、不派生子任务。
- 本次只验证静态原生展示和 headless 输入 fixture；真实 GPU、Linux、系统 IME、屏幕阅读器桥接以及上游大型插画像素等价不在完成声明中。
- 图标在窄区域内裁剪，极短高度可能没有剩余空间显示操作；调用方需提供足够高度或滚动容器。没有动画、服务、网络、栅格资产、动态 slot 拓扑或 Web aliases。

完整原始日志保留在本工作树忽略目录 `out/empty-result-*.log`，包括完整 Debug/Release CTest、专项、构建和 CDB 诊断。
