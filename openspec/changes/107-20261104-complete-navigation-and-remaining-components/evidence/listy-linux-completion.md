# Listy 原生补完与嵌套滚动（2026-11-12）

基于独立 checkout `48033d58061a540514ed7f60941e9880fc9b18d5`；实际 runtime 为 codex / gpt-6.1-sol / xhigh / priority。新增 typed 分组、交互 sticky 标题、按实际测量的虚拟区间、未挂载 key 定位；旧 API/default、受控分页、网格/横向/RTL、keyed 草稿和失败退役保留。缓存随已访问 key 增长，删除释放，不跨 key 复用编辑器。Theme 色彩更新不测量，字体几何更新不重跑工厂；只写共同 logical scene/resources。

新增双向 Table↔Listy→Select 和直接 Popover（内容仅 Input，无 Select）单 sync 合同。真实红例分别定位为混合嵌套 wheel 的外层抢占、富标题背景覆盖 hit fragment，以及 Popover 未消费现有 retained translation P 导致 anchor 落后 12dp；修复精确 owner 路由、分离 hit/surface，并复用既有 P 消费者。无新公共 phase/flag、额外 sync、窗口 pump 或 5 秒 callback 合同变更。

## 自动结果

Linux/GNU，CMakePresets / Ninja Multi-Config；同目录 Debug→Release 串行，每次 `-j2`。依赖源码副本、BINARY_DIR/generated/archive、libdecor/SDL prepare 和构建均在本 checkout 的 ignored `out/`。

- `cmake --build --preset linux-gcc-headless-{debug,release} --target rynui_portable_listy_component rynui_table_component_tests rynui_popover_component_tests rynui_popover_integration_tests rynui_popover_review_tests -j2`；精确选择 Listy/Table 与 Popover retained/triggers/boundaries/lifecycle/validation/gates/wheel/children/covered/clips/review 六项，共 **每配置 18/18 CTest**，每配置 **882 assertions = Listy 83 + Table 261 + Popover 538**。Listy 原 Select 组合范围 75 条，新 directPopover 双向新增 8 条；原断言未删减。
- `rynui_ant_design_reference_catalog_tests --catalog-identities-and-listy`：Debug/Release 各 **246 assertions**；`rynui.gallery_i18n_source` / `rynui.ant_design_gallery_catalog_generator` 各配置 **2/2 CTest**。保留 114 个 source 样例、490 个 frozen inventory targets 的完整身份顺序（包括旧 458 顺序），inventory 文件未改；2196 双语消息、73 catalog entries，只有 Listy 条目与 writer hash 更新。
- 默认 `rynui.ant_design_reference_catalog` 在 Debug 实际 **1 fail**：48033 原 overlay 的 Modal 已为 implemented，原测试仍 `require_partial(Modal)`。本次不改该无关 guard；新增显式精确 mode 保留原 snapshot/doc/categories 与 Listy 强 guard，默认全测试保留原路径。主线语义合入后必须用其最新 Modal/Card/ConfigProvider strong guards 跑完整默认测试，不能以本 mode 代替。

## 私有 Wayland 与 GPU

`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2` 后，使用 `tools/run_gallery_page_acceptance.py` → `tools/run_background_wayland.py`，独立 headless compositor/private DBus，`desktop_fallback=false`。实核运行的是指定 Mutter 51.1 候选，exe SHA256 `73952bc3087f68fcbbfdcebd82d5b72adf8975a404b58f6bb5b4d94ed7ea3bcd`，实际加载私有 libmutter-clutter SHA256 `4b2ffedf5161b3c21fec50bddf2bdf09a0b209b576dedaff64a6bb3c569983f4`。每次运行均核 `/proc/exe/maps`，未复用旧证明，未触及用户桌面。

| Release Vulkan/SPIR-V + system fonts | cases | checks | submitted frames | 协议 / GPU 捕获 |
|---|---:|---:|---:|---|
| 正常 Listy + Button | 2 | 169 | 34 | 主 xdg surface 69 次 frame request / 68 次 done，退出时 1 个 pending 随 surface 退役 |
| 正常 Popover | 1 | 12 | 1 | 主 surface 3 request / 2 done，退出时 1 个 pending |
| offscreen Listy | 1 | 27 | 75 | 20 条 GPU 捕获（原 8 + 新 9 次操作全部执行，另 3 条样例/roundtrip）；无正常 surface callback |
| offscreen Popover | 1 | 11 | 21 | 10 条 GPU 捕获；无正常 surface callback |

Listy normal helper 实际检查 200 行的有界首挂载、真实 wheel、远端组跳转、row/group 草稿与身份、virtual/sticky 切换、重排、空态恢复，以及真实 editor pointer capture + 合成 preedit 后离页清理。`listy_native=passed` 只在断言后输出。visual helper 保留原 8 操作，Listy 专属上限 17 实际覆盖新增 9；旧上限 8 的初次 visual pass **不计为新增 GPU 覆盖**。正常窗口与离屏 readback 分别计数，不互代。

新增代码已通过 clang-format 22.1.3 全仓自有源检查、catalog writer check、双语 source check、OpenSpec doctor/全量 strict 与 `git diff --check`。原始日志、JSON、源码冻结/hash、BMP 与复现包装均仅在自有 ignored `out/data-display-20261112/`。

本阶段只补充原 checked 36 的遗漏范围，**不关闭 63.3**；Table 动态 schema/span 与其最终类别组合仍在后续阶段。新增 group/sticky/virtual 未在 Windows/MSVC/D3D12 重验；物理系统 IME 候选窗、平台 AT、实际多显示器与桌面人工体验未验，合成 preedit 不代表这些平台结果。嵌套新组合的单 sync/输入合同由 HEADLESS 验证，本阶段未宣称其独立 native GPU 像素矩阵。

## main 语义合入复验

主线以 `f5bbd72d28b21949b674b9881a088ef10073f8ba` 为父基线，仅合入不可变 `a1fd5f587208e7a43b9d0c3f40ee2313c235b2b8` 相对 `48033d58061a540514ed7f60941e9880fc9b18d5` 的阶段差量。保留当前 Modal/Card/ConfigProvider/反馈 strong guards、109 inventory/CLI 和 Other helper；catalog 由当前 writer 重生。

- Linux/GNU `linux-gcc-headless` Debug→Release 串行 `-j2`，上述受影响组合每配置 **18/18**，完整默认 catalog、i18n、generator 和 109 inventory/protocol 每配置 **5/5**；显式 Listy mode 每配置 **246 assertions**。实际 compiled/source join 均为 73 catalog / 80 routes / 114 samples / 114 sources / 490 legacy targets；757 个差分 obligation 仍 pending，不是 paired pass。
- `linux-gcc-release` 实际 Vulkan/SPIR-V、system fonts：normal Listy+Button **2/169/34**，normal Popover **1/12/1**；hidden Listy **1/27/75**、**20** 条捕获记录（含完整 17 ops），hidden Popover **1/11/21**、**10** 条捕获记录。normal 主 surface 分别为 **69 request / 69 done** 和 **3 request / 2 done**（退出时一个 pending）；hidden 无正常 surface callbacks。`current.bmp` 工作文件不另计捕获。
- 四轮均经既有 `tools/run_background_wayland.py` 私有 Wayland/DBus，逐轮核实际 exe 与 Clutter SHA256 为上列私有候选，`desktop_fallback=false`，退出后自有进程零残留。包装 canary 首次因 Unix socket 路径过长失败；改用既有 helper 的短 `/tmp` 私有 socket 后重跑成功，原始失败保留在 out。
- clang-format **22.1.3** 检查、OpenSpec **2.0.0** doctor healthy / strict **109/0**、diff check 通过。所有 tasks checkbox 向量保持主线原状：本 change 不新增勾选，109 仍 **6/61**。Windows 与 Web/native paired 未在本次验证。

主线原始日志、JSON、捕获、环境指纹和冻结材料仅存 ignored `out/demo-differential/listy-*`。
