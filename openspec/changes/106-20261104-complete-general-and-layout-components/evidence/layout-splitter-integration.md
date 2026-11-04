# Layout / Splitter 主线集成

2026-11-04，Linux x86_64 / GCC 16.2.1，`linux-gcc-headless` Debug / Release，Ninja Multi-Config，构建最多 `-j2`。

- 顺序应用本地 Layout `9627c13f`、Splitter `9ba4aa42`；共享 Theme/服务/目录冲突作为同一阶段解决和验证。保留 FloatButton、typed Typography rich runs/双 MIME、Grid/Masonry、共享窗口逻辑宽度断点；三个 helper 与集成前逐字相同，CMake 与服务注册各一次。保留有限原生验收的 Wayland frame callback 同步。
- 构建 Shell/Splitter、两者独立 Gallery、Grid/Masonry、Theme algorithm/runtime/scope/public API、目录和页面模型相关目标。定向 CTest **Debug 54 项通过、Release 54/54 通过**；Debug 首轮 53/54，仅 source inventory 仍为旧 70，按真实目录改为 **77** 并补 Layout/Splitter API 断言，定向复验 **1/1**。未重跑全量 CTest。
- 覆盖 Shell 11 项、Splitter 16 项、Grid 7 项、Masonry 9 项；Theme/目录/生成/页面/依赖守卫共 11 项。原分支细节矩阵见 [Layout](layout-linux-headless.md) 与 [Splitter](splitter-linux-headless.md)，本集成不将 HEADLESS 计为真实窗口验收。
- 五份 Theme golden 在写入前通过结构化 JSON 与集成前 `d7600ec2` 比较：所有既有字段除 identity 外完全相同，仅新增 `floatButton`、`layout`、`splitter`。恢复完整 diagnostic 字节相等检查，保留 FloatButton 默认值/identity 独立断言。golden 是测试实际读取的确定性输入。
- 目录为 **73 项 / 7 类**，implemented **17**、partial **38**、planned **17**、deprecated **1**；General **4/4**、Layout **7/7** 原生合同实现。真实源码示例 **77**，冻结 legacy inventory **458** 与基线逐字不变。DOM/CSS/React 专属接口继续排除。
- 10 个公开头独立 C++20 `g++ -fsyntax-only -I include` 通过；HEADLESS backend 边界含 include/link 正反向守卫通过。clang-format 22.1.3：**876 文件 / 0 failures**；目录 `--check --self-test`、OpenSpec doctor、strict **106/106**、`git diff --check` 通过。

本阶段完成 4.1/4.2/6.1/6.2，任务 **12/16**；Linux 7.1/7.2 等待后续真实后台验收与最终 Release 构建，Windows 8.1/8.2 未测且保持未勾选。未操作用户桌面或系统鼠标键盘。构建/原始日志与诊断只在 ignored `out/106-final/`、`out/build/`；无 PR、无 archive。

定向 CTest 过滤器（Debug / Release 相同）：

```text
^rynui[.](shell_layout_|splitter_|grid_|masonry_|theme_algorithm$|portable[.](theme_runtime|theme_scope_component|theme_public_api|backend_boundaries)$|ant_design_reference_catalog$|gallery_page_model$|ant_design_gallery_catalog_(generator|contract)$|gallery_sample_source_(contract|generated)$)
```

## Linux 自动集成收口

同日，`linux-gcc` Debug，构建 `rynui_token_gallery` 与受影响 Theme/目录/页面/runner/clipboard targets（最多 `-j2`）。定向 CTest **15 项通过**：首轮 14/15，新增窗口序列的测试 helper 将“已经持有焦点，无需切换”误判为失败；修正焦点判断并将指针坐标计入文档滚动 translation 后，窗口序列单独复验 **1/1** 通过。没有跳过检查、放宽期限或回退桌面。

```text
ctest --preset linux-gcc-debug -R '^rynui[.]gallery_native_shell_splitter$' --output-on-failure
cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2
```

真实私有 Mutter/Wayland，Vulkan / SPIR-V / system fonts：Layout(Default) → Splitter(Dark) → Button(Dark+Compact)，**3 cases / 196 checks / 34 submitted frames / 34 frame callbacks**，`passed=true`、`return_code=0`、`desktop_fallback=false`，最后一次约 **4.97 秒**。保留原有逐帧同步、单帧最多 5 秒和总计 60 秒期限。

- Layout：真实窗口 1000→500 的共享 lg 断点，200→0 侧栏几何，零宽触发器 Enter/Space 展开/收起，保留输入草稿、隐藏时结束文本会话；受控侧栏 180↔64 回写，factory 保持一次，离页清理组件与文本编辑器。
- Splitter：经宿主合成分发实际指针 down/move/up/cancel，捕获建立与释放；Home/End/箭头、15% minimum/40% maximum 和相邻 minimum、相邻总量与无关面板守恒；lazy preview 保留实际尺寸、取消丢弃、释放提交；折叠恢复尺寸/草稿、destroyOnHidden 销毁并以新 generation 重建；带未完成 lazy 捕获离页后的组件/编辑器/会话/捕获清理。
- 复用此前已通过的 FloatButton 与 Typography→Grid→Masonry→Button GPU 证据（见 [前序集成](general-layout-integration.md)）；未重复完整原生矩阵或全量 CTest。新增几何和输入检查均在实际 GPU/窗口流程运行，不用通用页面挂载代替交互。

补强 maximum 覆盖后，未节拍版本在 Layout 阶段发生 frame callback 超时（两次普通复现和一次协议追踪复现均为 exit 1 / passed=false，非成功 case）；旧 frame callback 等待完整保留。协议记录显示 callback 与 WSI 的后续 presentation/FIFO 事件并非同一边界。presentation-time 试验在初始化中间提交收到 discarded，已撤回代码和额外 SDK 依赖，失败记录仅留 out。

最终只给有限验收宿主增加按实际 `SDL_DisplayMode::refresh_rate` 的单刷新周期动作节拍：继续分发 SDL 事件、不请求额外渲染，刷新率无效明确失败；节拍受原 frame deadline 与 60 秒总期限约束，未扩大期限。未改变普通 Gallery、Core、renderer、ABI、驱动或 compositor 设置，不宣称通用 renderer 的高速连续提交压力已验收。

```text
ctest --preset linux-gcc-debug -R '^rynui[.]gallery_native_(shell_splitter|general_layout|masonry_compact)$' --repeat until-fail:2 --output-on-failure
```

三条序列均连续两次通过，共 **6 次实际 GPU 调用通过**。最后一次分别为 General/Layout **4 cases / 122 checks / 16 frames / 16 callbacks**；Masonry Compact **4 / 83 / 11 / 11**；Layout/Splitter **3 / 196 / 34 / 34**，均 `passed=true`、`return_code=0`、`desktop_fallback=false`。额外两个真实 Gallery CPU 页与两个源码/生成合同 **4/4** 通过；显式 40% maximum 未被相邻 minimum 掩盖的断言实际通过。没有重跑全量 CTest 或完整原生矩阵。

主路径 Release Gallery 构建成功：`out/build/linux-gcc/examples/Release/rynui_token_gallery`，仅构建，未启动到用户桌面。原始日志、失败记录、JSON、BMP 和二进制均只留在 ignored `out/106-final/`、`out/gallery-pages/` 与 `out/build/`。

7.1/7.2 已完成，change106 为 **14/16**，剩余仅 **8.1/8.2 Windows 自动平台验收**，保持未勾选。输入是组件宿主合成分发；未验证系统物理输入、跨应用剪贴板消费端、用户桌面体验、跨显示器、系统 IME 或全部状态的像素矩阵。无人工待办、无 PR、无 archive。
